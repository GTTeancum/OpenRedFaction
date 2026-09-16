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
for special in [0,1]:
 for difficulty in range(5):
  u=machine();event=BASE;u.mem_write(event+0x2b8,w(600));u.mem_write(0x593e54,w(difficulty));u.mem_write(0x6460f8,struct.pack('<f',123));name=b'station_blowup' if special else b'Countdown_Begin';u.mem_write(event+0x1c,w(len(name),BASE+0x6000));u.mem_write(BASE+0x6000,name+b'\0')
  def hook(cpu,a,n,data):
   pass
  u.hook_add(UC_HOOK_CODE,hook);call(u,0x4bd600,ecx=event);remaining=struct.unpack('<f',u.mem_read(0x6460f8,4))[0];expected=600 if not special else [90,55,45,35,123][difficulty];assert remaining==expected
  results.append(dict(scope='begin',special=special,difficulty=difficulty,remaining=remaining))
for special in [0,1,2]:
 for armed in [0,1]:
  for remaining in [0,29,30,31]:
   u=machine();event=BASE;target=BASE+0x2000;trace=[];u.mem_write(event+0x2b8,bytes([armed,0,0,0]));u.mem_write(event+0x2bc,w(30));u.mem_write(event+0x29c,w(1,1,BASE+0x4000));u.mem_write(BASE+0x4000,w(101));u.mem_write(0x6460f8,struct.pack('<f',remaining));level=b'L17S2.rfl' if special else b'L15S4.rfl';name=b'countdown_sound' if special==2 else b'When_Countdown_Reaches';u.mem_write(0x645fe4,w(len(level),BASE+0x6000));u.mem_write(BASE+0x6000,level+b'\0');u.mem_write(event+0x1c,w(len(name),BASE+0x6100));u.mem_write(BASE+0x6100,name+b'\0')
   def hook(cpu,a,n,data):
    if a==0x4b6800:return_boundary(cpu,target)
    elif a in [0x46afa0,0x4c08e0]:return_boundary(cpu,0)
    elif a==0x4b8b70:trace.append('activation');return_boundary(cpu,0,12)
   u.hook_add(UC_HOOK_CODE,hook);call(u,0x4bd290,ecx=event);fire=remaining>0 and remaining<30 and (special!=1 or armed);assert bool(trace)==fire
   assert u.mem_read(event+0x2b9,1)[0]==int(fire);first=trace.copy();call(u,0x4bd290,ecx=event);assert trace==first
   results.append(dict(scope='threshold',special=special,armed=armed,remaining=remaining,fired=fire,armed_after=u.mem_read(event+0x2b8,1)[0]))
for remaining,dt in [(1,.5),(1,1),(1,1.5),(0,.5)]:
 u=machine();u.mem_write(0x6460f8,struct.pack('<f',remaining));u.mem_write(0x5a4014,struct.pack('<f',dt));u.mem_write(0x6460ec,w(1))
 def hook(cpu,a,n,data):
  if a==0x43331a:cpu.reg_write(UC_X86_REG_EIP,STOP)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x4332d2);after=struct.unpack('<f',u.mem_read(0x6460f8,4))[0];pulse=word(u,0x6460ec);assert after==max(0,remaining-dt);assert pulse==(3 if remaining>0 and remaining-dt<0 else 1)
 results.append(dict(scope='expiry_slice',remaining=remaining,dt=dt,after=after,flags=pulse))
for disabled in [0,1]:
 for empty in [0,1]:
  u=machine();event=BASE;second=BASE+0x400;trace=[];u.mem_write(0x6460ec,w(3))
  for obj in [event,second]:u.mem_write(obj+0x290,w(75,0,-1));u.mem_write(obj+0x2b0,w(disabled));u.mem_write(obj+0x29c,w(0 if empty else 1,1,BASE+0x4000))
  u.mem_write(BASE+0x4000,w(101))
  def hook(cpu,a,n,data):
   if a==0x4b6800:return_boundary(cpu,BASE+0x2000)
   elif a==0x4b8b70:trace.append(word(cpu,0x6460ec));return_boundary(cpu,0,12)
  u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b8ce0,ecx=event);assert trace==([] if empty else [3]);assert word(u,0x6460ec)==1;first=trace.copy();call(u,0x4b8ce0,ecx=second);assert trace==first
  results.append(dict(scope='over_pulse',disabled=disabled,empty=empty,flags_during_activation=first,flags_after=word(u,0x6460ec)))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Actual Begin and Reaches routines plus decrement slice execute; target activation/resolution intercepted; actual name predicates execute.','Difficulty4 fixture demonstrates out-of-range no-write branch, not a supported game setting.','Full mission persistence, HUD and special-name comparison internals not executed.'])
(ROOT/'artifacts/future-campaign-re/countdown.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'countdown scenarios')
