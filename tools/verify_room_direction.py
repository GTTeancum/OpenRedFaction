"""Original deterministic room-search direction generator 4e0c20."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
f32=lambda v:struct.unpack('<f',f(v))[0]
def machine(path):
 p=pefile.PE(str(path));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);u.mem_map(base,0x10000);return u
u=machine(exe);inputs=[];expected=[];rng=random.Random(0x4e0c20)
seeds=[[0,1,0],[0,-1,0],[0,0,0],[1,0,0],[0,0,1],[.0001,1,0],[-.0001,-1,0],[0,1,.00001]]
seeds += [[rng.uniform(-1,1) for _ in range(3)] for _ in range(128)]
for seed in seeds:
 direction=f(*seed);cosine=f32(.9753)
 for step in range(16):
  payload=direction+f(cosine);inputs.append(payload);u.mem_write(base,direction);u.mem_write(stack,w(stop,base)+f(cosine));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(0x4e0c20,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  direction=bytes(u.mem_read(base,12));expected.append(w(0)+direction)
  if cosine>-1:cosine=max(-1.,f32(cosine-f32(.13579)))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_collision_probe.exe'),'--room-direction'],input=b''.join(inputs))
for i,e in enumerate(expected):assert actual[i*16:(i+1)*16]==e,('PC',i,actual[i*16:(i+1)*16].hex(),e.hex())
x=machine(ROOT/'build/xbox/main.exe')
entry=int(re.search(r'_rf_collision_room_direction\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
for i,payload in enumerate(inputs):
 for alias in (False,True):
  output=base if alias else base+0x100;x.mem_write(base,payload);x.mem_write(stack,w(stop,base)+payload[12:]+w(output));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
  x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
  got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(output,12));assert got==expected[i],('NXDK',i,got.hex(),expected[i].hex())
report=dict(result='PASS',pc_cases=len(inputs),nxdk_cases=2*len(inputs),scope='Unmodified 4e0c20, basis/normalization/transform callees and x87 trig. Sixteen-step sequences, including vertical and non-unit inputs. No containing-room traversal yet.')
(ROOT/'artifacts/room-direction-verification.json').write_text(json.dumps(report,indent=2));print(report)
