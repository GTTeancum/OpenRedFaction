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
for limit,unlimited in [(2,0),(0,0),(0,1)]:
 u=machine();event=BASE;target=BASE+0x2000;u.mem_write(event,w(0x589a2c));u.mem_write(event+0x290,w(20,0,-1));u.mem_write(event+0x29c,w(1,1,BASE+0x4000));u.mem_write(BASE+0x4000,w(101));u.mem_write(event+0x2a8,w(11,22));u.mem_write(target+0x2c,w(333));u.mem_write(0x5a3ed8,w(1000));trace=[]
 def hook(cpu,a,n,data):
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x4b6870:return_boundary(cpu,event)
  elif a in [0x4b6800,0x46afa0]:return_boundary(cpu,target)
  elif a==0x4b8b70:trace.append(['event',*struct.unpack('<3I',cpu.mem_read(sp+4,12))]);return_boundary(cpu,0,12)
  elif a==0x46aba0:trace.append(['mover',*struct.unpack('<3I',cpu.mem_read(sp+4,12))]);return_boundary(cpu)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b80b0,(0,0x3f000000,limit,unlimited));assert word(u,event+0x2c8)==1000;call(u,0x4bb7a0,ecx=event);frames=[]
 for now in [1000,1499,1500,5000]:
  u.mem_write(0x5a3ed8,w(now));call(u,0x4bb7b0,ecx=event);frames.append(dict(now=now,count=word(u,event+0x2cc),deadline=word(u,event+0x2c8)))
 assert [f['count'] for f in frames]==([1,1,2,3] if unlimited else [1,1,2,2] if limit else [0,0,0,0])
 before=len(trace);call(u,0x4bb8a0,ecx=event);u.mem_write(0x5a3ed8,w(6000));call(u,0x4bb7b0,ecx=event);assert len(trace)==before;call(u,0x4bb7a0,ecx=event);call(u,0x4bb7b0,ecx=event);assert len(trace)==before+(2 if unlimited else 0)
 for i in range(0,len(trace),2):assert trace[i]==['event',0xffffffff,0xffffffff,1] and trace[i+1]==['mover',333,22,11]
 results.append(dict(scope='cyclic',limit=limit,unlimited=unlimited,frames=frames,trace=trace))
for hit in [0,1]:
 for pending in [0,1]:
  for disabled in [0,1]:
   u=machine();event=BASE;object=BASE+0x2000;target=BASE+0x3000;trace=[];u.mem_write(event,w(0x589c9c));u.mem_write(event+0x290,w(52,0,1500 if pending else -1));u.mem_write(event+0x2b0,w(disabled));u.mem_write(event+0x29c,w(2,2,BASE+0x4000));u.mem_write(BASE+0x4000,w(101,102));u.mem_write(object+0x7c,w(0x200000 if hit else 0));u.mem_write(0x5a3ed8,w(1000))
   def hook(cpu,a,n,data):
    sp=cpu.reg_read(UC_X86_REG_ESP);handle=word(cpu,sp+4)
    if a==0x40a0e0:return_boundary(cpu,object if handle==101 else 0)
    elif a==0x4b6800:return_boundary(cpu,target if handle==102 else 0)
    elif a==0x46afa0:return_boundary(cpu,0)
    elif a==0x4b8b70:trace.append(list(struct.unpack('<3I',cpu.mem_read(sp+4,12))));return_boundary(cpu,0,12)
   u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b8ce0,ecx=event);first=len(trace);call(u,0x4b8ce0,ecx=event);assert len(trace)==2*first;assert first==int(hit and not pending);assert word(u,object+0x7c)==(0x200000 if hit else 0)
   results.append(dict(scope='when_hit',hit=hit,pending=pending,disabled=disabled,trace=trace))
for invulnerable in [0,1]:
 for damage in [0,.0009,.001,1]:
  u=machine();obj=BASE;u.mem_write(obj+0x7c,w(4 if invulnerable else 0));bits=struct.unpack('<I',struct.pack('<f',damage))[0]
  def hook(cpu,a,n,data):
   if a==0x40a0e0:return_boundary(cpu,obj)
   elif a in [0x489327,0x487d1d]:cpu.reg_write(UC_X86_REG_EIP,STOP)
  u.hook_add(UC_HOOK_CODE,hook);call(u,0x4892c0,(101,bits,0,0,0,0,0,0));flags=word(u,obj+0x7c);assert bool(flags&0x200000)==(damage>=.001);call(u,0x487cf0,(obj,));assert word(u,obj+0x7c)==(4 if invulnerable else 0)
  results.append(dict(scope='damage_signal_prefix',invulnerable=invulnerable,damage=damage,flags_after_damage=hex(flags),flags_after_clear=hex(word(u,obj+0x7c))))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Actual cyclic factory/state/tick/timers and When_Hit base tick/poll execute.','Allocation, object resolution and outgoing activation effects intercepted.','Damage signal producer4892c0 prefix and clear487cf0 prefix execute; health mutation tail and authored factory reader not executed.'])
(ROOT/'artifacts/future-campaign-re/recurring-hit.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'recurring/hit scenarios')
