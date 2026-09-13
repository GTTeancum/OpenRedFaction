"""Bounded streaming VFX directory over actual projectile assets."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
B=0x30000000;ARCH=B+0x1000;OUT=B+0x2000;NAME=B+0x3000;HEAP=B+0x4000;BUF=B+0x5000;STACK=B+0xe000;STOP=B+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,65536)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
find=sym('rf_vpp_find');read=sym('rf_vpp_read');malloc=sym('malloc');free=sym('free')
word=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
state=dict(data=b'',name='',live=False,fail_malloc=False,reads=0,fail_read=0)
def hook(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);arg=[word(sp+4+i*4) for i in range(5)];result=0
 if a==find:
  assert arg[0]==ARCH and bytes(x.mem_read(arg[1],len(state['name'])+1))==state['name'].encode()+b'\0'
  x.mem_write(arg[2],state['name'].encode().ljust(64,b'\0')+w(0,len(state['data'])))
 elif a==read:
  state['reads']+=1;assert arg[0]==ARCH
  if state['reads']==state['fail_read']:result=-1
  else:
   chunk=state['data'][arg[2]:arg[2]+arg[4]];assert len(chunk)==arg[4] and arg[4]<=128
   x.mem_write(arg[3],chunk)
 elif a==malloc:
  assert not state['live'] and 0<arg[0]<=84
  if state['fail_malloc']:result=0
  else:state['live']=True;x.mem_write(HEAP,b'\xa5'*arg[0]);result=HEAP
 elif a==free:
  if arg[0]:assert arg[0]==HEAP and state['live'];state['live']=False
 u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
for a in (find,read,malloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(sym(name),STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def opening(budget):
 state['reads']=0;return run('rf_vfx_directory_open',[ARCH,NAME,budget,OUT])
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp');assets=json.loads((root/'artifacts/vfx-header.json').read_text())['assets'];reports=[];failures=0
for asset in assets:
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
 state.update(data=data,name=asset['name']);x.mem_write(NAME,asset['name'].encode()+b'\0');x.mem_write(OUT,bytes(216))
 at=asset['header_bytes'];chunks=[]
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);assert length>=4 and at+4+length<=len(data)
  chunks.append((tag,at+8,length-4));at+=4+length
 assert at==len(data);budget=216+len(chunks)*12
 assert opening(budget-1)==0xfffffffc and not state['live'] and bytes(x.mem_read(OUT,216))==bytes(216);failures+=1
 state['fail_malloc']=True;assert opening(budget)==0xffffffff;state['fail_malloc']=False
 assert not state['live'] and bytes(x.mem_read(OUT,216))==bytes(216);failures+=1
 state['fail_read']=len(chunks)+2;assert opening(budget)==0xffffffff;state['fail_read']=0
 assert not state['live'] and bytes(x.mem_read(OUT,216))==bytes(216);failures+=1
 assert opening(budget)==0 and word(OUT+208)==len(chunks) and word(OUT+212)==budget
 got=bytes(x.mem_read(OUT+76,128))+w(len(chunks),budget)+bytes(x.mem_read(HEAP,len(chunks)*12))
 assert got[136:]==b''.join(w(*c) for c in chunks)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-directory',str(root/'Installed_Game/meshes.vpp'),asset['name']]);assert pc==got
 for i,(tag,offset,size) in enumerate(chunks):
  x.mem_write(BUF,b'\xa5'*128)
  assert run('rf_vfx_chunk_read',[OUT,i,size,BUF,1])==0xfffffffc and bytes(x.mem_read(BUF,128))==b'\xa5'*128
  amount=min(size,128);assert run('rf_vfx_chunk_read',[OUT,i,0,BUF,amount])==0 and bytes(x.mem_read(BUF,amount))==data[offset:offset+amount]
 snapshot=bytes(x.mem_read(OUT,216));assert opening(budget)==0xfffffffc and bytes(x.mem_read(OUT,216))==snapshot
 run('rf_vfx_directory_close',[OUT]);run('rf_vfx_directory_close',[OUT]);assert not state['live'] and bytes(x.mem_read(OUT,216))==bytes(216)
 # Corrupt a stored record length and truncate the final payload.
 for broken in [data[:-1],data[:asset['header_bytes']+4]+w(0xffffffff)+data[asset['header_bytes']+8:],data[:asset['header_bytes']+4]+w(3)+data[asset['header_bytes']+8:]]:
  state['data']=broken;assert opening(budget)==0xfffffffe and not state['live'] and bytes(x.mem_read(OUT,216))==bytes(216);failures+=1
 reports.append(dict(name=asset['name'],records=len(chunks),directory_bytes=budget,types=[struct.pack('<I',c[0]).decode() for c in chunks]))
report=dict(result='PASS',assets=reports,nxdk_failure_cases=failures,total_records=sum(r['records'] for r in reports),scope='PC actual archive and compiled NXDK directory with supplied archive/heap services; independently walked physical record lengths. Exact/short budgets, allocation and second-pass I/O rollback, reopen/repeated close, payload boundaries, oversized/undersized lengths and truncation. All record types retained. No semantic object/track parsing or native XEMU.')
(root/'artifacts/vfx-directory.json').write_text(json.dumps(report,indent=2));print(report)
