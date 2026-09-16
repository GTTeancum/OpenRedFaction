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
# Actual registered-event array, activation, no-op83 virtual action, timer and link walking.
for kinds,delays,flags in [([],[],[]),([55,83,15,83,83],[0,0,0,.25,0],[0,0,0,0,1]),([83,83],[0,0],[0,0])]:
 u=machine();trace=[];objects=[]
 for i,kind in enumerate(kinds):
  obj=BASE+i*0x400;links=BASE+0x10000+i*16;objects.append(obj)
  u.mem_write(obj,w(0x589c9c));u.mem_write(obj+0x290,w(kind,struct.unpack('<I',struct.pack('<f',delays[i]))[0],0xffffffff,2,2,links));u.mem_write(obj+0x2b0,w(flags[i]));u.mem_write(links,w(100+i*2,101+i*2))
 u.mem_write(0x856470,w(len(objects),len(objects),BASE+0x20000));u.mem_write(BASE+0x20000,w(*objects));u.mem_write(0x5a3ed8,w(1000))
 def hook(cpu,a,n,data):
  if a==0x4b65c0:
   sp=cpu.reg_read(UC_X86_REG_ESP);trace.append(list(struct.unpack('<3I',cpu.mem_read(sp+4,12))));return_boundary(cpu)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x45b900);immediate=list(trace)
 expected=[[100+i*2+j,0xffffffff,0xffffffff] for i,k in enumerate(kinds) if k==83 and not delays[i] and not flags[i] for j in range(2)]
 assert immediate==expected
 states=[dict(kind=k,source=word(u,objects[i]+0x2ac),actor=word(u,objects[i]+0x2a8),deadline=word(u,objects[i]+0x298)) for i,k in enumerate(kinds)]
 trace.clear();u.mem_write(0x5a3ed8,w(1250))
 for i,k in enumerate(kinds):
  if k==83:call(u,0x4b8ce0,ecx=objects[i])
 delayed=list(trace);expected=[[100+i*2+j,0xffffffff,0xffffffff] for i,k in enumerate(kinds) if k==83 and delays[i] and not flags[i] for j in range(2)]
 assert delayed==expected
 trace.clear();call(u,0x45b900);assert trace==immediate # No intrinsic one-shot completion latch.
 results.append(dict(scope='full completion broadcast / actual event activation and delayed tick',kinds=kinds,delays=delays,flags=flags,immediate=immediate,at_1250=delayed,states=states,repeated_broadcast=trace.copy()))
# Full start/stop routines, real descriptor lookup, external subsystem calls captured.
for present,blocked,lookup,count in [(0,0,1,2),(1,1,1,2),(1,0,0,2),(1,0,1,0),(1,0,1,2)]:
 u=machine();trace=[];entity=BASE+0x30000;camera=BASE+0x33000;desc=BASE+0x36000
 u.mem_write(0x5cb054,w(entity if present else 0));u.mem_write(0x7c75d4,w(camera));u.mem_write(camera+0xc4,w(0x11223344));u.mem_write(camera+0xd8,w(0x428c0000))
 u.mem_write(0x8723b4,w(0x872128));u.mem_write(0x5cb2ec,w(0x5cb060));u.mem_write(0x596140,w(0x42c80000));u.mem_write(desc,w(3696,count));u.mem_write(desc+0x85c,w(0x42700000))
 u.mem_write(0x645fa8,w(1,1,BASE+0x39000));u.mem_write(BASE+0x39000,w(desc));u.mem_write(entity+0x144,b'\x55'*24)
 hooks={0x427020:(1,blocked),0x505df0:(0,0),0x505c70:(2,0),0x5059f0:(0,0),0x527cd0:(0,0),0x434220:(0,0),0x434570:(0,0),0x42e8a0:(0,0),0x45b3f0:(2,0),0x45b950:(0,0),0x42cd60:(1,0),0x4ad8a0:(1,0),0x4b05d0:(0,0),0x430fc0:(1,0),0x41ae70:(2,0),0x4290d0:(1,0),0x5001d0:(2,0),0x52fc60:(1,0),0x40ddf0:(1,0),0x48a660:(1,0),0x500290:(2,0),0x409f30:(0,0),0x527d40:(0,0)}
 def hook(cpu,a,n,data):
  if a in hooks:
   argc,value=hooks[a];sp=cpu.reg_read(UC_X86_REG_ESP);trace.append(dict(address=hex(a),args=list(struct.unpack('<'+'I'*argc,cpu.mem_read(sp+4,argc*4))) if argc else []));return_boundary(cpu,value)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x45bb00,(3696 if lookup else 9999,))
 active=word(u,0x645320);rate=word(u,0x596140);fov=word(u,camera+0xd8);start_trace=trace.copy()
 assert active==(desc if present and not blocked and lookup else 0)
 if present and not blocked and (not lookup or count):assert rate==0x41fa0000
 else:assert rate==0x42c80000
 if present and not blocked and lookup and count:assert fov==0x42700000
 trace.clear();call(u,0x45bda0)
 assert word(u,0x645320)==0
 if active:assert word(u,camera+0xd8)==0x42b40000 and bytes(u.mem_read(entity+0x144,24))==bytes(24)
 results.append(dict(scope='full start/stop, real descriptor lookup, intercepted external effects',present=present,blocked=blocked,lookup=lookup,point_count=count,active_after_start=hex(active),float_global_after_start=hex(rate),camera_fov_after_start=hex(fov),start_trace=start_trace,stop_trace=trace.copy(),float_global_after_stop=hex(word(u,0x596140))))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['External start/stop subsystem effects captured, not executed.','No cutscene file reader, camera interpolation, vehicle path, full timeline or actual campaign was run.'])
(ROOT/'artifacts/future-campaign-re/cutscene-lifecycle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'cutscene lifecycle/broadcast scenarios')
