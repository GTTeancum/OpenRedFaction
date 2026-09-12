"""Execute compiled NXDK resource loading with supplied archive reads and heap."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
B=0x30000000;N=B+0x1000;M=B+0x2000;O=B+0x5000;S=B+0x700000;STOP=S+0x1000
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,0x800000)
mapping=(ROOT/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,open_file,find,read_file,malloc,calloc,free=map(sym,('rf_static_model_tags_open','rf_static_model_tags_close','rf_model_file_open','rf_vpp_find','rf_vpp_read','malloc','calloc','free'))
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v));r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
live={};allocations=reads=0;fail_alloc=fail_read=0;cursor=B+0x10000;raw=b'';filename='';cases=0

def hook(cpu,a,size,ctx):
 global allocations,reads,cursor
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4);status=0
 if a in (malloc,calloc):
  allocations+=1;count=arg(0)*(arg(1) if a==calloc else 1)
  if allocations!=fail_alloc:
   status=cursor;cursor+=(count+15)&~15;assert cursor<S;live[status]=count;cpu.mem_write(status,bytes(count) if a==calloc else b'\xa5'*count)
 elif a==free:
  if arg(0):count=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*count)
 elif a==find:cpu.mem_write(arg(2),filename.encode().ljust(64,b'\0')+w(0,len(raw)))
 else:
  reads+=1;offset,count=arg(2),arg(4)
  if reads==fail_read:status=0xffffffff
  else:
   assert offset+count<=len(raw),(filename,offset,count,len(raw));cpu.mem_write(arg(3),raw[offset:offset+count])
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,arg(-1))
for a in (find,read_file,malloc,calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(a,STOP,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def hash_bytes(data):
 h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 return h
def check(budget,wanted,expected):
 global allocations,reads,cursor,cases
 assert not live;allocations=reads=0;cursor=B+0x10000;x.mem_write(O,bytes(12));status=call(entry,M,budget,O)
 assert status==wanted,(filename,status,wanted)
 if status:assert bytes(x.mem_read(O,12))==bytes(12) and not live
 else:
  ptr,count,cost=struct.unpack('<3I',x.mem_read(O,12))
  assert cost==12+sum(live.values()) and cost==budget and count*104==len(expected)
  assert bytes(x.mem_read(ptr,count*104))==expected if count else expected==b''
 total_allocations,total_reads=allocations,reads
 call(close,O);call(close,O);assert bytes(x.mem_read(O,12))==bytes(12) and not live;cases+=1
 return total_allocations,total_reads
from inspect_models import inspect
archive=next(a for a in json.loads((ROOT/'artifacts/inventory.json').read_text())['files'] if a['path']=='meshes.vpp');rows=[]
for e in archive['vpp']['entries']:
 if not e['name'].lower().endswith('.v3m'):continue
 filename=e['name']
 with (ROOT/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 lod=next(s for s in inspect(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
 expected=b''
 for i in range(lod['props']):
  a=lod['attachment_offset']+i*100;expected+=raw[a:a+68]+bytes(4)+raw[a+68:a+100]
 x.mem_write(N,filename.encode()+b'\0');assert call(open_file,M,B,N)==0
 budget=12+len(expected);count,read_count=check(budget,0,expected);check(budget-1,0xfffffffc,expected)
 for fail_alloc in range(1,count+1):check(budget,0xffffffff,expected)
 fail_alloc=0
 for fail_read in range(1,read_count+1):check(budget,0xffffffff,expected)
 fail_read=0
 rows.append(dict(model=filename,bytes=budget,reads=read_count))
report=dict(result='PASS',cases=cases,models=len(rows),records=rows,scope='Actual compiled NXDK file decoder and tag owner with only heap and VPP reads supplied. Every installed static model versus serialized attachments; exact/short budgets and every allocation/attachment-read failure; output unchanged and no leaks. No native XEMU or live scene claim.')
(ROOT/'artifacts/static-tag-owner-nxdk.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
