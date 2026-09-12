"""Owned archive-backed static metadata, exact budgets and cleanup."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from inspect_models import inspect
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
r=lambda d,o=0:struct.unpack_from('<I',d,o)[0]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x100000);N=B+0x1000;O=B+0x2000;S=B+0xe0000;STOP=S+0x1000
mapping=(ROOT/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,open_file,bound,sphere,malloc,free=map(sym,('rf_static_model_metadata_open','rf_static_model_metadata_close','rf_model_file_open','rf_model_file_static_bound_sphere','rf_model_file_collision_sphere','malloc','free'))
read=lambda a:r(x.mem_read(a,4));live={};allocations=0;fail_alloc=0;fail_stage=0;rows=[];bounds=b'';filename='';cases=0

def text(a):
 out=bytearray()
 while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
 return out.decode()
def hook(cpu,a,size,context):
 global allocations
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+i*4);result=0
 if a==malloc:
  allocations+=1;n=arg(0);assert n==(8792 if allocations==1 else len(rows)*48)
  if allocations!=fail_alloc:
   result=B+0x10000+allocations*0x10000;assert result not in live;live[result]=n;cpu.mem_write(result,b'\xa5'*n)
 elif a==free:
  if arg(0):n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
 elif a==open_file:
  assert text(arg(2))==filename
  if fail_stage==1:result=0xfffffffd
  else:
   cpu.mem_write(arg(0),bytes(8792));cpu.mem_write(arg(0)+76,w(len(rows),1))
   for i in range(len(rows)):cpu.mem_write(arg(0)+84+i*20,w(0x43535048))
 elif a==bound:
  if fail_stage==2:result=0xffffffff
  else:cpu.mem_write(arg(1),bounds)
 else:
  if fail_stage==3:result=0xffffffff
  else:cpu.mem_write(arg(2),rows[arg(1)])
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,arg(-1))
for a in (open_file,bound,sphere,malloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(a,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def check(budget,status_expected=0,pc=False):
 global allocations,cases
 assert not live;allocations=0;x.mem_write(O,bytes(96));x.mem_write(N,filename[:-4].encode()+b'.v3d\0')
 status=call(entry,B,N,budget,O);assert status==status_expected,(filename,hex(status),hex(status_expected))
 if status:
  assert bytes(x.mem_read(O,96))==bytes(96) and not live
  wanted=w(status)+bytes(92)
 else:
  retained=96+48*len(rows);peak=retained+8792
  wanted=w(0)+filename.encode().ljust(64,b'\0')+bounds+w(len(rows),retained,peak)+b''.join(rows)
  actual=w(0)+bytes(x.mem_read(O,80))+bytes(x.mem_read(O+84,12))+(bytes(x.mem_read(read(O+80),48*len(rows))) if rows else b'')
  assert actual==wanted and len(live)==int(bool(rows)),filename
  snapshot=bytes(x.mem_read(O,96));assert call(entry,B,N,budget,O)==0xfffffffc and bytes(x.mem_read(O,96))==snapshot
 if pc:
  actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_model_file_probe.exe'),'--static-metadata',str(ROOT/'Installed_Game/meshes.vpp'),filename[:-4]+'.v3d',str(budget)])
  assert actual==wanted,(filename,'PC')
 call(close,O);call(close,O);assert not live and bytes(x.mem_read(O,96))==bytes(96);cases+=1
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text());archive=next(a for a in inventory['files'] if a['path']=='meshes.vpp')
verified_bounds={row['model']:row for row in json.loads((ROOT/'artifacts/model-static-bounds.json').read_text())['records']}
records=[]
for e in archive['vpp']['entries']:
 if not e['name'].lower().endswith('.v3m'):continue
 filename=e['name'];bounds=struct.pack('<4f',*verified_bounds[filename]['sphere'])
 with (ROOT/'Installed_Game/meshes.vpp').open('rb') as stream:stream.seek(e['offset']);data=stream.read(e['size'])
 rows=[]
 for section in inspect(data)['sections']:
  if section['type']=='0x43535048':
   raw=data[section['offset']+8:section['offset']+8+44];assert section['declared']==44
   rows.append(raw[:24]+bytes(4)+raw[24:])
 check(65536,pc=True);retained=96+len(rows)*48;peak=retained+8792
 check(peak,pc=True);check(peak-1,0xfffffffc,pc=True)
 records.append(dict(filename=filename,count=len(rows),retained=retained,peak=peak))
# Synthetic nonempty rows cover both allocations and failure after final storage.
filename='fixture.v3m';bounds=struct.pack('<4f',0,0,0,2);rows=[bytes(28)+w(-1)+struct.pack('<4f',0,0,0,1)]
for fail_alloc in (1,2):check(65536,0xffffffff)
fail_alloc=0
for fail_stage in (1,2,3):check(65536,0xfffffffd if fail_stage==1 else 0xffffffff)
fail_stage=0
report=dict(result='PASS',models=len(records),cases=cases,max_retained=max(v['retained'] for v in records),max_peak=max(v['peak'] for v in records),total_spheres=sum(v['count'] for v in records),scope='PC actual archive metadata versus compiled NXDK composition with supplied preverified directory/bounds/CSPH reads. All427 static models, exact/short budgets, source release, duplicate open and repeated close; allocation/read failures. Geometry/textures/full original loader/native Xbox archive I/O excluded.',records=records)
(ROOT/'artifacts/static-model-metadata.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
