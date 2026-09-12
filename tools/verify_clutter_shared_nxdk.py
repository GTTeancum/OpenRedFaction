"""Run linked NXDK shared-base lifetime paths with only a supplied heap."""
import json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
ib=p.OPTIONAL_HEADER.ImageBase;x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
B=0x30000000;x.mem_map(B,0x100000);N=B;RESOURCE=B+0x100;SHARED=B+0x200;SPHERES=B+0x300
D=B+0x1000;MAT=B+0x1100;O=B+0x1200;R=B+0x80000;L=B+0x84000;UID=B+0x85000;S=B+0xe0000;STOP=S+0x1000
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,reginit,listinit,malloc,calloc,free=map(sym,('rf_clutter_shared_static_base_open','rf_clutter_shared_static_base_close','rf_object_registry_init','rf_object_list_init','malloc','calloc','free'))
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
read=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
live={};cursor=B+0x10000;allocations=0;fail=0
def hook(cpu,a,size,ctx):
 global cursor,allocations
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+i*4);result=0
 if a==free:
  if arg(0):n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
 else:
  allocations+=1;n=arg(0)*(arg(1) if a==calloc else 1)
  if allocations!=fail:
   result=cursor;cursor+=(n+15)&~15;assert cursor<R
   live[result]=n;cpu.mem_write(result,bytes(n) if a==calloc else b'\xa5'*n)
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
for a in (malloc,calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.reg_write(UC_X86_REG_FPCW,0x27f);x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S)
 x.emu_start(a,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
cases=0
for count in (0,1,4):
 for flags in (0,32):
  assert not live;cursor=B+0x10000;allocations=0;fail=0
  x.mem_write(N,b'FiXtUrE.v3m\0');x.mem_write(N+64,b'fixture.v3d\0')
  x.mem_write(RESOURCE,bytes(28)+struct.pack('<4f',0,0,0,2)+w(SPHERES if count else 0,count))
  rows=b''.join(b'sphere\0'.ljust(28,b'\0')+w(-1)+struct.pack('<4f',i*.25,0,0,.5) for i in range(count))
  if rows:x.mem_write(SPHERES,rows)
  x.mem_write(SHARED,w(N,RESOURCE,0));x.mem_write(D,w(N+64,1,2,flags,0,-1)+struct.pack('<13f',3,2,-1,1,0,0,0,1,0,0,0,1,-1))
  x.mem_write(MAT,struct.pack('<3f',.25,.5,2));x.mem_write(O,bytes(12));x.mem_write(UID,w(-1));call(reginit,R);call(listinit,L)
  retained=532+(max(count,1)*24 if flags else 0);peak=retained+count*24
  args=[SHARED,D,R,L,UID,0,0,1,MAT,peak,O]
  for slot in (0,4):
   args[-1]=O+slot;assert call(entry,*args)==0 and read(O+slot)
   owner=read(O+slot);assert read(owner+524)==retained and read(owner+528)==peak
  assert read(SHARED+8)==2 and read(L+8)==2
  if flags:assert read(read(O)+440)!=read(read(O+4)+440)
  args[-1]=O+8;args[-2]=peak-1;assert call(entry,*args)==0xfffffffc
  assert read(O+8)==0 and read(SHARED+8)==2 and read(L+8)==2
  assert call(close,SHARED,O,R,L)==0 and read(SHARED+8)==1
  assert call(close,SHARED,O+4,R,L)==0 and read(SHARED+8)==0 and not live
  assert call(close,SHARED,O+4,R,L)==0 and read(R+12292)==1024
  assert not rows or bytes(x.mem_read(SPHERES,len(rows)))==rows
  cases+=1
# Fail every allocation of the four-sphere, physics-enabled constructor.
for fail in (1,2,3):
 allocations=0;args[-2]=peak
 status=call(entry,*args);assert status==(0xfffffffc if fail==3 else 0xffffffff),(fail,status)
 assert read(O+8)==0 and read(SHARED+8)==0 and read(R+12292)==1024 and not live
 cases+=1
report=dict(result='PASS',cases=cases,scope='Linked NXDK code under x87 control0x27f; only malloc/calloc/free supplied. Zero/one/four spheres, physics0/20, two concurrent borrowers, distinct bodies, exact/short budgets, balanced releases, unchanged source spheres and all three allocation failures. No native XEMU/live scene claim.')
(root/'artifacts/clutter-shared-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
