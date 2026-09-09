"""Complete original attached translation propagation, including collection and bounds."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);base=0x30000000;u.mem_map(base,0x400000)
obj=base;controllers=base+4096;keys=base+16384;arrays=base+32768;stack=base+0x300000;stop=base+0x3ff000
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
rng=random.Random(0x46bbe0);results=[];changed=forced=roundtrips=multi=0
port_cases=[];port_expected=[]
def port_pose(blob):
 return b''.join(blob[offset:offset+size] for offset,size in [(0x7c,4),(0x180,4),(0x238,12),(0x244,36),(0xe4,12),(0x3c,12),(0xf0,12),(0x144,12),(0x48,36),(0xfc,36),(0x120,36),(0x190,12),(0x19c,12)])
for n in range(2000):
 count=n%3;force=n%2;dt=f32(rng.choice([.25,1/60,.03]));radius=f32(rng.choice([-1.,0.,.25,8.]));flags=rng.choice([0,0x8000000]);handle=0x12340000
 current=[f32(rng.uniform(-1000,1000)) for _ in range(3)];rest=[f32(rng.uniform(-1000,1000)) for _ in range(3)];matrix=[f32(rng.uniform(-1,1)) for _ in range(9)]
 initial=bytearray([0xa5]*0x298);struct.pack_into('<I',initial,0x2c,handle);struct.pack_into('<I',initial,0x7c,flags);struct.pack_into('<3f',initial,0xe4,*current);struct.pack_into('<3f',initial,0x238,*rest);struct.pack_into('<9f',initial,0x244,*matrix);struct.pack_into('<f',initial,0x180,radius)
 u.mem_write(obj,bytes(initial));u.mem_write(0x7394cc,struct.pack('<I',obj));u.mem_write(0x64e63c,struct.pack('<I',controllers if count else 0x64e3b0));u.mem_write(0x64ecb9,bytes(2));u.mem_write(0x5a4014,struct.pack('<f',dt))
 contributions=[];dirty=False;port_contributions=[]
 for i in range(count):
  ptr=controllers+i*1024;key=keys+i*128;arr=arrays+i*256;controller=bytearray(1024);first=[f32(rng.uniform(-10,10)) for _ in range(3)];pending=[f32(rng.uniform(-100,100)) for _ in range(3)]
  control_flags=rng.choice([0,8,0x80000000]);struct.pack_into('<I',controller,0x318,control_flags);struct.pack_into('<I',controller,0x28c,ptr+1024 if i+1<count else 0x64e3b0)
  struct.pack_into('<3I',controller,0x29c,1,1,arr);u.mem_write(arr,struct.pack('<I',key));u.mem_write(key,bytes(4)+struct.pack('<3f',*first)+bytes(112));struct.pack_into('<3f',controller,0xf0,*pending)
  for j,offset in enumerate([0x2cc,0x2c0]):
   # At most four contributions total: stay inside original four-controller record.
   refs=[rng.choice([handle,0xffffffff,0x56780000])];p=arr+32+j*32;u.mem_write(p,struct.pack('<I',*refs));struct.pack_into('<3I',controller,offset,1,1,p)
   if refs[0]==handle and (j==0 or not flags&0x8000000):
    contributions.append([f32(a-b) for a,b in zip(pending,first)]);dirty|=bool(control_flags&0x80000008);port_contributions.append(struct.pack('<6fI',*first,*pending,control_flags))
  u.mem_write(ptr,bytes(controller))
 u.mem_write(stack,struct.pack('<2I',stop,force));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x46bbe0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 want=bytearray(initial)
 if contributions and (force or dirty):
  changed+=1;forced+=force;multi+=len(contributions)>1;target=list(rest)
  for delta in contributions:target=[f32(a+b) for a,b in zip(target,delta)]
  struct.pack_into('<I',want,0x7c,flags|0x4000000)
  for offset in [0x48,0xfc,0x120]:struct.pack_into('<9f',want,offset,*matrix)
  if force:
   velocity=[0.,0.,0.];pending=target;committed=target
   for offset in [0x3c,0xe4]:struct.pack_into('<3f',want,offset,*target)
  else:
   velocity=[f32(f32(a-b)/dt) for a,b in zip(target,current)];pending=[f32(a+f32(v*dt)) for a,v in zip(current,velocity)];committed=current;roundtrips+=pending!=target
  struct.pack_into('<3f',want,0x144,*velocity);struct.pack_into('<3f',want,0xf0,*pending)
  struct.pack_into('<3f',want,0x190,*[f32(min(a,b)-radius) for a,b in zip(committed,pending)]);struct.pack_into('<3f',want,0x19c,*[f32(max(a,b)+radius) for a,b in zip(committed,pending)])
 actual=bytes(u.mem_read(obj,len(want)));assert actual==want,(n,'object mutation',[(hex(i),actual[i:i+4].hex(),want[i:i+4].hex()) for i in range(0,len(want),4) if actual[i:i+4]!=want[i:i+4]])
 port_cases.append(struct.pack('<2If',len(port_contributions),force,dt)+port_pose(initial)+b''.join(port_contributions)+bytes(28*(4-len(port_contributions))));port_expected.append(port_pose(actual))
 results.append(dict(case=n,controllers=count,contributions=len(contributions),force=force,dirty=dirty))
report=dict(result='PASS',cases=len(results),updated=changed,forced=forced,multiple_contributions=multi,velocity_roundtrip_differences=roundtrips,scope='Complete unchanged 46bbe0, stack probe, collector, handle lookup, vector/matrix and bounds helpers. Translation-only without flag 800 orientation override. Both lists, stale/missing/duplicate handles, multiple controllers, dirty gating, forced/normal poses and positive/nonpositive radius. Whole mover bytes checked. Not rotation, collision response or C port.',results=results)
(root/'artifacts/group-propagation-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
