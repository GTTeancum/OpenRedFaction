"""Original cutscene completion broadcast and lifecycle boundary evidence; no port build."""
import sys,struct,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'local/python'))
(ROOT/'artifacts/future-campaign-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXE=ROOT/'Installed_Game/RF.exe';SHA=hashlib.sha256(EXE.read_bytes()).hexdigest()
assert SHA=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
IMAGE=pefile.PE(str(EXE)).get_memory_mapped_image()
BASE=0x30000000;STACK=BASE+0x7e000;STOP=BASE+0x7f000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(IMAGE)+4095)//4096*4096);u.mem_write(0x400000,IMAGE);u.mem_map(BASE,0x80000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
def word(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def call(u,entry,args=(),ecx=0):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP

def return_boundary(u,value=0,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);ret=word(u,sp);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
results=[]
for path_present in [0,1]:
 for now in [999,1000,2965,10000]:
  u=machine();desc=BASE;point=desc+8;camera=BASE+0x2000;path=BASE+0x3000;trace=[]
  u.mem_write(0x645320,w(desc));u.mem_write(desc,w(123,2));u.mem_write(point,w(456));u.mem_write(point+4,struct.pack('<3f',1,2,7));u.mem_write(point+16,w(-1,-1));u.mem_write(desc+0x808,w(0,0,10000,1000,-1,point,path if path_present else 0))
  u.mem_write(0x644f10,w(1,1,BASE+0x4000));u.mem_write(BASE+0x4000,w(camera));u.mem_write(camera+4,struct.pack('<3f',4,5,6));u.mem_write(0x5a3ed8,w(now));u.mem_write(0x5a4014,struct.pack('<f',.25))
  def hook(cpu,a,n,unused):
   sp=cpu.reg_read(UC_X86_REG_ESP)
   if a==0x45b877:cpu.reg_write(UC_X86_REG_EIP,STOP)
   elif a==0x530060:
    trace.append(dict(effect='path_sample',fraction=struct.unpack('<f',cpu.mem_read(sp+8,4))[0]));return_boundary(cpu,0,8)
   elif a==0x45b3f0:trace.append(dict(effect='next_point',index=word(cpu,sp+8)));return_boundary(cpu)
   elif a in [0x45bda0,0x45b900,0x434190,0x435450]:trace.append(dict(effect=hex(a)));return_boundary(cpu)
  u.hook_add(UC_HOOK_CODE,hook);call(u,0x45b67a)
  active=u.mem_read(desc+0x858,1)[0];deadline=word(u,desc+0x818)
  if now==10000:assert trace==[dict(effect='next_point',index=1)]
  elif path_present and now>=1000:
   assert active==1 and deadline==now+1965 and len(trace)==1
   assert abs(trace[0]['fraction']-.25*.9827237725257874/2)<1e-7
   assert list(struct.unpack('<3f',u.mem_read(desc+0x824,12)))==[4,5,6]
   # Move exactly to path deadline; pre-timer invalidated means no reinitialization.
   u.mem_write(0x5a3ed8,w(deadline));call(u,0x45b67a);assert u.mem_read(desc+0x858,1)[0]==0 and word(u,desc+0x818)==0xffffffff
   assert len(trace)==2 # final sample occurs before deactivation, fraction not clamped here
  else:assert not trace and active==0
  results.append(dict(path=path_present,initial_time=now,path_deadline=deadline,trace=trace))
# Endpoint order; bypasses external stop effects, records actual call order.
u=machine();desc=BASE;u.mem_write(0x645320,w(desc));u.mem_write(desc+4,w(1));u.mem_write(desc+0x808,w(0,0,1000));u.mem_write(0x5a3ed8,w(1000));trace=[]
def hook(cpu,a,n,unused):
 if a==0x45b877:cpu.reg_write(UC_X86_REG_EIP,STOP)
 elif a in [0x45bda0,0x45b900,0x434190,0x435450]:trace.append(hex(a));return_boundary(cpu)
u.hook_add(UC_HOOK_CODE,hook);call(u,0x45b67a);assert trace==['0x45bda0','0x45b900','0x434190','0x435450'];results.append(dict(endpoint=trace))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Executes original tick slice45b67a..45b877; entry camera setup and rendering tail not run.','Spline sampling, next-point setup and external completion effects intercepted.','Actual timer arithmetic, phase writes, camera vector copy and FPU fraction arithmetic executed.'])
(ROOT/'artifacts/future-campaign-re/cutscene-phases.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'phase/endpoint cases')
