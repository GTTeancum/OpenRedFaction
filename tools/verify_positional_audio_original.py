"""Execute original positional sound math with original vector helpers intact."""
import hashlib,itertools,json,struct,sys,subprocess,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;m.mem_map(base,65536)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
u=lambda *v:struct.pack('<'+'I'*len(v),*v)
f32=lambda v:struct.unpack('<f',f(v))[0]
xp=pefile.PE(str(root/'build/xbox/main.exe'));xd=xp.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(xp.OPTIONAL_HEADER.ImageBase,(len(xd)+4095)//4096*4096);x.mem_write(xp.OPTIONAL_HEADER.ImageBase,xd);x.mem_map(base,65536)
entry=int(re.search(r'_rf_audio_position\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=[];commands=bytearray();expected=bytearray()
for near,far,factor,volume,axis,sign in itertools.product([1.,4.,16.],[32.,64.],[0.,.5,1.,2.],[0.,.25,1.],range(3),[-1.,1.]):
 for distance in [0.,near*.5,near,near*2,far,far+1]:
  pos=[0.,0.,0.];pos[axis]=sign*distance
  m.mem_write(0x1754160,f(0,0,0));m.mem_write(0x1753c28,f(1,0,0))
  m.mem_write(0x1cd3ba8+0x24,f(near,far,factor));m.mem_write(base,f(*pos));m.mem_write(base+32,b'\xa5'*8)
  m.mem_write(stack,u(stop,0,base,base+32,base+36)+f(volume));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f)
  m.emu_start(0x505740,stop,count=1000);assert m.reg_read(UC_X86_REG_EIP)==stop
  got=struct.unpack('<2f',m.mem_read(base+32,8))
  if distance>far:want=(0.,0.)
  else:
   den=(distance/near-1)*factor+1
   gain=volume if distance<near or den==0 else f32(volume/den)
   want=(sign if axis==0 and distance else 0.,min(volume,max(0.,gain)))
  assert got==want,(near,far,factor,volume,pos,got,want)
  wire=f(*pos,0,0,0,1,0,0,near,far,factor,volume);commands.extend(wire);expected.extend(f(*got))
  x.mem_write(base,wire);x.mem_write(stack,u(stop,base,base+12,base+24)+f(near,far,factor,volume)+u(base+64))
  x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
  x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
  assert bytes(x.mem_read(base+64,8))==f(*got),('NXDK',pos,got,bytes(x.mem_read(base+64,8)))
  cases.append([near,far,factor,volume,*pos,*got])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--position'],input=commands)
assert actual==expected,'PC positional output differs'
report=dict(result='PASS' ,cases=len(cases),original_sha256=digest,x87_control='0x27f',
 scope='Original 505740 and all vector/clamp helpers execute unchanged; axis-aligned distances with fixed listener/right vector, near/far boundaries, gain and pan. Exact shared C PC/NXDK comparison; no general-vector rounding claim.',
 output_sha256=hashlib.sha256(json.dumps(cases).encode()).hexdigest())
(root/'artifacts/positional-audio-original.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
