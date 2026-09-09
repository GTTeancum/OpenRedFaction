"""Full original translation ticks and position commits over controlled key paths."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);keys=base+4096;array=base+8192;stack=base+60000;stop=base+64000
def call(address):
 u.mem_write(stack,struct.pack('<2I',stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 try:u.emu_start(address,stop,count=100000)
 except Exception as error:raise RuntimeError(hex(u.reg_read(UC_X86_REG_EIP))) from error
 assert u.reg_read(UC_X86_REG_EIP)==stop
def setup(reverse=False,accel=0.,decel=0.,speed_mode=False,timer=-1):
 obj=bytearray(1024);struct.pack_into('<3I',obj,0x29c,2,2,array);struct.pack_into('<I',obj,0x2e8,1)
 for offset in [0x2d8,0x2dc,0x2e0,0x2e4,0x31c,0x320,0x324,0x328]:struct.pack_into('<i',obj,offset,-1)
 current,target=(1,0) if reverse else (0,1);struct.pack_into('<2i',obj,0x2f8,current,target);struct.pack_into('<i',obj,0x30c,target);struct.pack_into('<i',obj,0x310,timer)
 struct.pack_into('<I',obj,0x318,(0 if reverse else 0x2000)|(0x400 if speed_mode else 0))
 for offset in [0x3c,0xe4,0xf0]:struct.pack_into('<3f',obj,offset,8.*current,0,0)
 u.mem_write(base,bytes(obj));u.mem_write(array,struct.pack('<2I',keys,keys+128))
 for i in range(2):
  key=bytearray(128);struct.pack_into('<3f',key,4,8.*i,0,0);struct.pack_into('<4f',key,0x38,2.,2.,accel,decel)
  for offset in [0x48,0x4c,0x50]:struct.pack_into('<i',key,offset,-1)
  u.mem_write(keys+i*128,bytes(key))
 u.mem_write(0x5a4014,struct.pack('<f',.25));u.mem_write(0x5a3ed8,struct.pack('<i',1000));u.mem_write(0x64ecb9,bytes(2))
def state():
 b=bytes(u.mem_read(base,1024));return dict(position=list(struct.unpack_from('<3f',b,0xe4)),pending=list(struct.unpack_from('<3f',b,0xf0)),speed=struct.unpack_from('<f',b,0x2f4)[0],elapsed=struct.unpack_from('<f',b,0x300)[0],distance=struct.unpack_from('<f',b,0x304)[0],current=struct.unpack_from('<i',b,0x2f8)[0],next=struct.unpack_from('<i',b,0x2fc)[0],flags=struct.unpack_from('<I',b,0x318)[0])
results=[]
for reverse in [False,True]:
 for speed_mode in [False,True]:
  setup(reverse,speed_mode=speed_mode,timer=0);frames=[];step=.5 if speed_mode else 1.;arrival=int(8/step)
  for frame in range(arrival+2):
   call(0x469800);s=state();distance=min((frame+1)*step,8.);want=8.-distance if reverse else distance
   assert s['pending']==[want,0.,0.],(reverse,speed_mode,frame,s)
   if frame<arrival:assert s['current']==(1 if reverse else 0) and s['next']==(0 if reverse else 1),(frame,s)
   else:assert s['next']==-1 and s['current']==(0 if reverse else 1),(frame,s)
   call(0x46a8f0);assert state()['position']==s['pending'];frames.append(s)
  results.append(dict(reverse=reverse,speed_mode=speed_mode,frames=frames))
# Held timer: integrators advance, but position remains at the initial point.
held=[]
for timer in [-1,2000]:
 setup(timer=timer)
 for frame in range(4):
  call(0x469800);s=state();assert s['speed']==4. and s['elapsed']==(frame+1)*.25 and s['distance']==frame+1 and s['pending']==[0.,0.,0.],s
 held.append(dict(timer=timer,state=s))
acceleration=[]
for reverse in [False,True]:
 setup(reverse,accel=1.,decel=1.,timer=-1)
 for frame,(speed,distance) in enumerate([(1.,.25),(2.,.75),(3.,1.5),(4.,2.5),(4.,3.5),(4.,4.5),(4.,5.5),(4.,6.5),(3.,7.25),(2.,7.75),(1.,8.)]):
  call(0x469800);s=state();assert s['speed']==speed and s['distance']==distance and s['elapsed']==(frame+1)*.25,(reverse,frame,s)
 acceleration.append(dict(reverse=reverse,state=s))
# Braking threshold zero is accepted only in the backward branch.
asymmetry=[]
for reverse in [False,True]:
 setup(reverse,decel=4.,timer=-1);call(0x469800);s=state()
 want=struct.unpack('<f',struct.pack('<f',.4))[0] if reverse else 4.
 assert s['speed']==want,(reverse,s);asymmetry.append(dict(reverse=reverse,state=s))
report=dict(result='PASS',scope='Complete unchanged 469800 and 46a8f0 on two-key paths; original helpers, disabled sound handles and no external key links. Both directions, duration/speed modes, exact positions, delayed arrival transition, held timers, acceleration/braking and zero-threshold directional asymmetry. No C port, scene propagation or rotation playback.',paths=results,held_timers=held,acceleration=acceleration,braking_asymmetry=asymmetry)
(root/'artifacts/group-translation-original.json').write_text(json.dumps(report,indent=2));print(dict(result=report['result'],paths=len(results),frames=sum(len(r['frames']) for r in results),held_timer_ticks=8,acceleration_ticks=22,asymmetry_ticks=2))
