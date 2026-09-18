"""Execute49d330; only material getter returns are supplied through x87 ABI."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;stub=base+0xf100;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<%dI'%len(v),*v);pack=lambda v:struct.pack('<%df'%len(v),*v)
u.mem_write(stub,b'\xd9\x05'+w(stub+32)+b'\xc3');u.mem_write(stub+16,b'\xd9\x05'+w(stub+36)+b'\xc3')
def hook(cpu,address,size,data):
 if address==0x4687e0:cpu.reg_write(UC_X86_REG_EIP,stub)
 elif address==0x468810:cpu.reg_write(UC_X86_REG_EIP,stub+16)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x49d330);commands=[];expected=[];moving_contact_cases=0
for case in range(512):
 flags=0x8000003f|(0x200 if case%31==0 else 0);reset=int(case%37==0);stateflags=0x12341000
 n=([0,1,0],[1,0,0],[.6,.8,0],[.48,.64,.6])[case%4];pos=[rng.uniform(-10,10) for _ in range(3)]
 vel=[rng.uniform(-8,8) for _ in range(3)];angular=[rng.uniform(-2,2) for _ in range(3)];momentum=[rng.uniform(-2,2) for _ in range(3)]
 if case%11==0:vel=[0,-.1,0];angular=[0,0,0];momentum=[0,0,0]
 if case%13==0:vel=[0,-5,0];angular=[0,0,0];momentum=[0,0,0]
 tensor=[1,0,0,0,1,0,0,0,1] if case&2 else [2,.2,.1,.2,1,.3,.1,.3,.5]
 point=[pos[i]-(.2*n[i])+(rng.uniform(-.1,.1) if case&4 else 0) for i in range(3)]
 v=[(1,2,10)[case%3],(.04,.6,1)[case%3],(.1,0,.5)[case%3],.5,(0,.5,2)[(case//3)%3],0]+pos+vel+angular+momentum+tensor+point+n+[0,-9.8,0]
 command=w(flags,reset,stateflags)+pack(v);v=struct.unpack('<36f',command[12:]);u.mem_write(base,bytes(0x1500));u.mem_write(stack-0x100,bytes(0x200))
 for off,data in [(0x98,v[:1]),(0x88,v[1:2]),(0x90,v[2:3]),(0xe4,v[6:9]),(0x144,v[9:12]),(0x150,v[12:15]),(0x15c,v[15:18]),(0xc0,v[18:27]),(0x1b4,v[27:30]),(0x1c0,v[30:33])]:u.mem_write(base+off,pack(data))
 u.mem_write(base+0x1a8,w(flags,stateflags));u.mem_write(base+0x1ec,w(reset));u.mem_write(stub+32,pack(v[3:5]));u.mem_write(0x7c7058,pack(v[33:36]));u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 initial_object=bytes(u.mem_read(base,0x1500))
 u.emu_start(0x49d330,stop,count=20000);assert u.reg_read(UC_X86_REG_EIP)==stop
 stationary_object=bytes(u.mem_read(base,0x1500))
 # Execute the real routine again with only the recorded counterpart velocity
 # changed. Compare every object byte, masking only that supplied input field.
 for surface_velocity in ([2,0,0],[0,3,0],[-4,-2,1]):
  u.mem_write(base,initial_object);u.mem_write(base+0x1d8,pack(surface_velocity))
  u.mem_write(stack-0x100,bytes(0x200));u.mem_write(stack,w(stop,base))
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x49d330,stop,count=20000);assert u.reg_read(UC_X86_REG_EIP)==stop
  moving_object=bytearray(u.mem_read(base,0x1500))
  assert moving_object[0x1d8:0x1e4]==pack(surface_velocity),('contact velocity mutated',case)
  moving_object[0x1d8:0x1e4]=stationary_object[0x1d8:0x1e4]
  assert moving_object==stationary_object,('moving contact changed response',case,surface_velocity)
  moving_contact_cases+=1
 u.mem_write(base,stationary_object)
 expected.append(bytes(u.mem_read(base+0x88,4))+bytes(u.mem_read(base+0x144,24))+bytes(u.mem_read(base+0x15c,12))+bytes(u.mem_read(base+0x1a8,4))+bytes(u.mem_read(base+0x1ec,4))+bytes(u.mem_read(base+0x1ac,4)));commands.append(command)
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_physics_probe.exe'),'--solid-contact'],input=b''.join(commands))
for case,want in enumerate(expected):
 got=actual[case*52:(case+1)*52]
 assert got==want,('contact mismatch',case,[(i,struct.unpack('<f',got[i:i+4])[0],struct.unpack('<f',want[i:i+4])[0]) for i in range(0,52,4) if got[i:i+4]!=want[i:i+4]])
native_cases=0
if '--nxdk' in sys.argv:
 pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));binary=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(binary)+4095)//4096*4096);x.mem_write(origin,binary);x.mem_map(base,0x10000)
 entry=int(re.search(r'_rf_physics_solid_contact\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
 for case,command in enumerate(commands):
  flags,reset,stateflags=struct.unpack('<3I',command[:12]);v=struct.unpack('<36f',command[12:]);state=bytearray([0xa5]*308)
  for off,items in [(12,v[:1]),(0,v[1:2]),(8,v[2:3]),(88,v[6:9]),(184,v[9:12]),(196,v[12:15]),(208,v[15:18]),(52,v[18:27])]:state[off:off+len(items)*4]=pack(items)
  state[272:280]=w(flags,stateflags);state[300:304]=w(reset);x.mem_write(base,bytes(state));x.mem_write(base+0x2000,pack(v[27:36]));x.mem_write(base+0x2100,w(0xdeadbeef))
  x.mem_write(stack,struct.pack('<IIIIIffI',stop,base,base+0x2000,base+0x200c,base+0x2018,v[3],v[4],base+0x2100));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
  x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
  want=expected[case]
  for off,a,b in [(0,0,4),(184,4,16),(196,16,28),(208,28,40),(272,40,44),(300,44,48),(276,48,52)]:state[off:off+b-a]=want[a:b]
  got=bytes(x.mem_read(base,308));assert got==state,('NXDK mismatch',case,[(i,got[i:i+4].hex(),state[i:i+4].hex()) for i in range(0,308,4) if got[i:i+4]!=state[i:i+4]])
  decision=0 if reset or flags&0x200 or want[:4]==pack(v[1:2]) else (2 if struct.unpack('<I',want[40:44])[0]&0x18000000==0x18000000 else 1)
  assert bytes(x.mem_read(base+0x2100,4))==w(decision),('response decision',case)
  native_cases+=1
report=dict(result='PASS',cases=len(expected),nxdk_cases=native_cases,moving_contact_cases=moving_contact_cases,scope='Original nonrigid contact and counterpart-velocity invariance, explicit surface coefficients; no random-normal/vehicle/contact query/scheduler')
(ROOT/'artifacts/physics-solid-contact.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
