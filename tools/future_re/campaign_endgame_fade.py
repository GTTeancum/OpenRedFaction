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
for gate in [0,11,18]:
 for duration,steps in [(1.5,[.5,.5,.5,.1]),(1.5,[1.5,.01]),(1.5,[2]),(1,[.25,.25,.25,.25,.01])]:
  u=machine();player=BASE;u.mem_write(0x7c75d4,w(player));u.mem_write(0x5967a4,w(0));u.mem_write(0x630064,w(gate or 13));trace=[];colors=[];call(u,0x4a73e0,(player,struct.unpack('<I',struct.pack('<f',duration))[0],0x43e9a0));initial=word(u,player+0x11d4)
  def hook(cpu,a,n,data):
   sp=cpu.reg_read(UC_X86_REG_ESP)
   if a in [0x4a4940,0x4a4920]:return_boundary(cpu,0)
   elif a==0x434190:trace.append(list(struct.unpack('<2I',cpu.mem_read(sp+4,8))));return_boundary(cpu)
   elif a==0x50cf80:colors.append(list(struct.unpack('<4I',cpu.mem_read(sp+4,16))));return_boundary(cpu)
   elif a in [0x432eb1,0x432ee7]:cpu.reg_write(UC_X86_REG_EIP,STOP)
  u.hook_add(UC_HOOK_CODE,hook);frames=[]
  for dt in steps:
   u.mem_write(0x5a4014,struct.pack('<f',dt));u.reg_write(UC_X86_REG_EBX,player);u.reg_write(UC_X86_REG_EDI,0);call(u,0x432d2e);frames.append(dict(dt=dt,alpha=struct.unpack('<f',u.mem_read(player+0x11d4,4))[0],callback_count=len(trace)))
  assert initial==0x33d6bf95
  assert trace==([[19,0]] if gate else [])
  if gate:assert frames[-1]['alpha']==0
  else:assert frames[-1]['alpha']==255
  if len(steps)>1:assert all(f['callback_count']==0 for f in frames[:-1])
  results.append(dict(gate=gate,duration=duration,frames=frames,colors=colors,state_calls=trace))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Actual4a73e0 scheduling,432d2e..432eb1 fade slice, actual state gate434460 and callback43e9a0 executed.','Player predicates, color setter and state request intercepted; no render output/control playback.','Fixture ordinary player flags0 only; special fade-in/death flags not tested.'])
(ROOT/'artifacts/future-campaign-re/endgame-fade.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'endgame fade cases')
