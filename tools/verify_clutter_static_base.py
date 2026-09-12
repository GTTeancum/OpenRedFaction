"""Owned archive-backed static metadata, exact budgets and cleanup."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from inspect_models import inspect
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
r=lambda d,o=0:struct.unpack_from('<I',d,o)[0]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x100000);N=B+0x1000;O=B+0x2000;S=B+0xe0000;STOP=S+0x1000
mapping=(ROOT/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,open_file,bound,sphere,malloc,free=map(sym,('rf_clutter_static_base_open','rf_clutter_static_base_close','rf_model_file_open','rf_model_file_static_bound_sphere','rf_model_file_collision_sphere','malloc','free'))
calloc=sym('calloc');reginit=sym('rf_object_registry_init');listinit=sym('rf_object_list_init');R=B+0x80000;L=B+0x84000;UID=B+0x85000;D=B+0x86000;M=B+0x87000
read=lambda a:r(x.mem_read(a,4));live={};allocations=0;fail_alloc=0;fail_stage=0;rows=[];bounds=b'';filename='';cases=0

def text(a):
 out=bytearray()
 while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
 return out.decode()
def hook(cpu,a,size,context):
 global allocations
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+i*4);result=0
 if a in (malloc,calloc):
  allocations+=1;n=arg(0)*(arg(1) if a==calloc else 1);assert n in (532,96,8792,24,max(24,len(rows)*24),len(rows)*48),n
  if allocations!=fail_alloc:
   result=B+0x10000+allocations*0x10000;assert result not in live;live[result]=n;cpu.mem_write(result,(b'\0' if a==calloc else b'\xa5')*n)
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
for a in (open_file,bound,sphere,malloc,calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(a,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def check(budget,status_expected=0,pc=False,flags=0x20):
 global allocations,cases
 assert not live;allocations=0;call(reginit,R);call(listinit,L);x.mem_write(UID,w(-1));x.mem_write(O,w(0))
 authored=filename[:-4]+'.v3d';x.mem_write(N,authored.encode()+b'\0')
 x.mem_write(D,w(N,1,2,flags,0,-1)+struct.pack('<13f',0,0,0,1,0,0,0,1,0,0,0,1,-1));x.mem_write(M,struct.pack('<3f',.25,.5,2))
 status=call(entry,B,D,R,L,UID,0,0,1,M,budget,O)
 assert status==status_expected,(filename,hex(status),hex(status_expected));owner=read(O)
 wanted=w(status,int(bool(owner)),read(UID),read(R+12296),read(R+12292),read(L+8))
 if owner:
  raw=bytearray(x.mem_read(owner,532));meta=read(owner+12);used=read(owner+444);sphere_data=bytes(x.mem_read(read(owner+440),used*24)) if used else b''
  retained=532+96+len(rows)*48+used*24;peak=max(532+96+8792+len(rows)*48,retained+len(rows)*24)
  assert r(raw,524)==retained and r(raw,528)==peak<=budget
  for offset in (0,12,116):raw[offset:offset+4]=w(1)
  raw[108:116]=w(1,1);raw[440:444]=w(int(used>0))
  wanted+=raw+sphere_data+bytes(x.mem_read(meta,80))+bytes(x.mem_read(meta+84,12))+(bytes(x.mem_read(read(meta+80),len(rows)*48)) if rows else b'')
 closed=call(close,O,R,L);wanted+=w(closed,read(R+12296),read(R+12292),read(L+8),read(R+8192+((read(R+12288)+read(R+12292)-1)%1024)*4))
 assert closed==0 and not live and read(O)==0 and read(L+8)==0 and read(R+12292)==1024
 assert read(UID)==(0xfffffffe if allocations and fail_alloc!=1 else 0xffffffff)
 call(close,O,R,L);assert not live
 if pc:
  actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_model_file_probe.exe'),'--static-base',str(ROOT/'Installed_Game/meshes.vpp'),authored,str(budget),str(flags)])
  assert actual==wanted,(filename,'PC',[(i,actual[i:i+4].hex(),wanted[i:i+4].hex()) for i in range(0,min(len(actual),len(wanted)),4) if actual[i:i+4]!=wanted[i:i+4]][:8])
 cases+=1
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
   raw=data[section['offset']+8:section['offset']+52];rows.append(raw[:24]+bytes(4)+raw[24:])
 check(65536,pc=True);peak=532+96+8792+len(rows)*48
 check(peak,pc=True,flags=0);check(peak-1,0xfffffffc,pc=True)
 records.append(dict(filename=filename,count=len(rows),retained=628+len(rows)*48+max(1,len(rows))*24,peak=peak))
filename='fixture.v3m';bounds=struct.pack('<4f',0,0,0,2);rows=[bytes(28)+w(-1)+struct.pack('<4f',0,0,0,1)]
for fail_alloc in range(1,7):check(65536,0xfffffffc if fail_alloc==6 else 0xffffffff)
fail_alloc=0
for fail_stage in (1,2,3):check(65536,0 if fail_stage==1 else 0xffffffff)
fail_stage=0
report=dict(result='PASS',models=len(records),cases=cases,max_retained=max(v['retained'] for v in records),max_peak=max(v['peak'] for v in records),scope='PC actual archive-backed static base objects versus compiled NXDK full ownership composition, with supplied preverified file directory/bounds/CSPH reads only. Real model metadata, registration, physics and cleanup; all427 static files, enabled/disabled physics, exact/short budgets, six allocation failures, missing model and read failures. No geometry/textures/effects or live scene/native XEMU claim.',records=records)
(ROOT/'artifacts/clutter-static-base.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
