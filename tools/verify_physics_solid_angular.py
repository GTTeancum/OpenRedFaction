"""Original generic-body angular block49fbe6..49fd84; actual arithmetic callees."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000);pack=lambda v:struct.pack('<%df'%len(v),*v)
rng=random.Random(0x49fbe6);commands=[];expected=[]
for case in range(512):
 flags=(2 if case&1 else 0)|(0x1000000 if case&2 else 0)
 angle=rng.uniform(-3,3);c=math.cos(angle);s=math.sin(angle)
 basis=[c,0,s,0,1,0,-s,0,c] if case&4 else [1,0,0,0,1,0,0,0,1]
 tensor=[2,.2,.1,.2,1,.3,.1,.3,.5] if case&8 else [1,0,0,0,1,0,0,0,1]
 values=[(0,1/60,.1,1)[(case//16)%4],(.01,0,.5,2)[(case//64)%4]]
 values += [rng.uniform(-25,25) for _ in range(6)]+[rng.uniform(-10,10) for _ in range(3)]+tensor+basis
 command=struct.pack('<I',flags)+pack(values);values=struct.unpack('<29f',command[4:])
 u.mem_write(base,bytes(0x1500));u.mem_write(stack-0x200,bytes(0x400))
 for off,v in [(0x1b0,values[:1]),(0x8c,values[1:2]),(0x150,values[2:5]),(0x15c,values[5:8]),(0x174,values[8:11]),(0xc0,values[11:20]),(0xfc,values[20:29])]:u.mem_write(base+off,pack(v))
 u.mem_write(base+0x1a8,struct.pack('<I',flags));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49fbe6,0x49fd84,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49fd84
 expected.append(bytes(u.mem_read(base+0x15c,12))+bytes(u.mem_read(base+0x150,12))+bytes(u.mem_read(base+0x120,36)));commands.append(command)
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_physics_probe.exe'),'--solid-angular'],input=b''.join(commands))
maximum=0
for i,want in enumerate(expected):
 got=actual[i*60:(i+1)*60]
 assert got[:24]==want[:24],('momentum/angular mismatch',i,got[:24].hex(),want[:24].hex())
 errors=[abs(a-b) for a,b in zip(struct.unpack('<9f',got[24:]),struct.unpack('<9f',want[24:]))];maximum=max(maximum,max(errors))
 assert got[24:]==want[24:],('orientation mismatch',i,max(errors),struct.unpack('<9f',got[24:]),struct.unpack('<9f',want[24:]))
native_cases=0;native_maximum=0
if '--nxdk' in sys.argv:
 pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));binary=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(binary)+4095)//4096*4096);x.mem_write(origin,binary);x.mem_map(base,0x10000)
 entry=int(re.search(r'_rf_physics_solid_angular_propose\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
 for case,command in enumerate(commands):
  flags=struct.unpack('<I',command[:4])[0];v=struct.unpack('<29f',command[4:]);state=bytearray([0xa5]*308)
  for off,items in [(4,v[1:2]),(196,v[2:5]),(208,v[5:8]),(232,v[8:11]),(52,v[11:20]),(112,v[20:29])]:state[off:off+len(items)*4]=pack(items)
  state[272:276]=struct.pack('<I',flags);x.mem_write(base,bytes(state));stop=base+0xf000
  x.mem_write(stack,struct.pack('<IIf',stop,base,v[0]));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
  x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
  got=bytes(x.mem_read(base,308));want=expected[case]
  state[208:220]=want[:12];state[196:208]=want[12:24];state[148:184]=got[148:184]
  assert got==state,('NXDK body mismatch',case,[(i,struct.unpack('<f',got[i:i+4])[0],struct.unpack('<f',state[i:i+4])[0]) for i in range(0,308,4) if got[i:i+4]!=state[i:i+4]])
  errors=[abs(a-b) for a,b in zip(struct.unpack('<9f',got[148:184]),struct.unpack('<9f',want[24:]))];native_maximum=max(native_maximum,max(errors))
  assert got[148:184]==want[24:],('NXDK orientation mismatch',case,max(errors))
  native_cases+=1
report=dict(result='PASS',cases=len(expected),nxdk_cases=native_cases,maximum_orientation_error=maximum,nxdk_maximum_orientation_error=native_maximum,scope='Momentum, angular velocity and predicted orientation bit-exact; no contact/scene scheduling')
(ROOT/'artifacts/physics-solid-angular.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
