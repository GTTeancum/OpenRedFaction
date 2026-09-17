"""Execute original49d280 and NXDK full solid-pose acceptance; no rendering claim."""
import hashlib,json,math,random,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
original=ROOT/'Installed_Game/RF.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 pe=pefile.PE(str(path));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(im)+4095)//4096*4096);u.mem_write(origin,im)
 u.mem_map(0x30000000,0x10000);return u
u=load(original);x=load(ROOT/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_physics_solid_advance\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x49d280);pack=lambda v:struct.pack('<%df'%len(v),*v)
for case in range(128):
 angle=rng.uniform(-3,3);c=math.cos(angle);s=math.sin(angle)
 basis=pack([c,0,s,0,1,0,-s,0,c]);position=pack([rng.uniform(-20,20) for _ in range(3)])
 tensor=pack([2,.2,.1,.2,1,.3,.1,.3,.5]);radius=pack([rng.uniform(0,3)])
 body=bytearray(308);body[16:52]=tensor;body[100:112]=position;body[148:184]=basis;body[244:248]=radius
 u.mem_write(base,bytes(0x1000));x.mem_write(base,bytes(0x1000))
 for off,data in [(0x9c,tensor),(0xf0,position),(0x120,basis),(0x180,radius)]:u.mem_write(base+off,data)
 u.mem_write(stack,struct.pack('<II',stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x49d280,stop,count=20000);assert u.reg_read(UC_X86_REG_EIP)==stop
 x.mem_write(base,bytes(body));x.mem_write(stack,struct.pack('<IIffII',stop,base,.1,1,base+0x400,base+0x500))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 for native,orig,size in [(88,0xe4,12),(112,0xfc,36),(52,0xc0,36),(248,0x190,12),(260,0x19c,12),(292,0x1cc,4)]:
  expected=bytes(u.mem_read(base+orig,size));body[native:native+size]=expected
  assert bytes(x.mem_read(base+native,size))==expected,('mismatch',case,native)
 assert bytes(x.mem_read(base,308))==body,('unexpected body write',case)
 assert bytes(x.mem_read(base+0x400,36))==bytes(u.mem_read(base+0x48,36)),('public basis',case)
 assert bytes(x.mem_read(base+0x500,4))==bytes(4)
report=dict(result='PASS',cases=128,scope='Original49d280 versus NXDK full acceptance: pose, world tensor, bounds, contact fraction, public basis; partial branch covered by shared unit checks only')
(ROOT/'artifacts/physics-solid-advance.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
