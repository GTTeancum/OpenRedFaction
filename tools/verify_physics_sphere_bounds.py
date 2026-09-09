"""Compare complete original 4a0cb0 sphere radius/bounds against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
rng=random.Random(0x4a0cb0);commands=[];expected=[]
for case in range(1000):
 count=(0,1,2,16,17,32)[case%6]
 position=[rng.uniform(-100,100) for _ in range(3)] if case>=12 else [-0.0,0.0,-0.0]
 spheres=b''.join(floats(*[rng.uniform(-25,25) for _ in range(3)],rng.uniform(0,10),-1)+pack(0x12340000+i) for i in range(count))
 raw=pack(count)+floats(*position)+spheres
 seed=bytearray([0xa5]*0x170);seed[0x5c:0x68]=raw[4:16];seed[0xfc:0x108]=pack(count,count,base+0x2000)
 u.mem_write(base,bytes(seed));u.mem_write(base+0x2000,spheres or bytes(24));u.mem_write(stack,pack(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4a0cb0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 radius=bytes(u.mem_read(base+0xf8,4));bounds=bytes(u.mem_read(base+0x108,24))
 seed[0xf8:0xfc]=radius;seed[0x108:0x120]=bounds
 assert bytes(u.mem_read(base,len(seed)))==seed
 if count:assert bytes(u.mem_read(base+0x2000,len(spheres)))==spheres
 commands.append(raw);expected.append(radius+bounds+pack(0))
original_cases=len(commands)
sphere=floats(1,2,3,1,-1)+pack(0)
for index in range(7):
 for bad in (float('inf'),float('nan')):
  raw=bytearray(pack(1)+floats(1,2,3)+sphere);at=4+index*4;raw[at:at+4]=floats(bad)
  commands.append(bytes(raw));expected.append(bytes([0xa5])*28+pack(0xfffffffc))
for pos,center,radius in [([0,0,0],[0,0,0],-1),([0,0,0],[3.4028234663852886e38]*3,1),([0,0,0],[3.4028234663852886e38,0,0],3.4028234663852886e38),([3.4028234663852886e38,0,0],[0,0,0],3.4028234663852886e38)]:
 commands.append(pack(1)+floats(*pos,*center,radius,-1)+pack(0));expected.append(bytes([0xa5])*28+pack(0xfffffffc))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--bounds'],input=b''.join(commands))
assert len(actual)==len(expected)*32
for i,want in enumerate(expected):assert actual[i*32:(i+1)*32]==want,('PC',i,actual[i*32:(i+1)*32].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_physics_spheres_bounds\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(raw,want) in enumerate(zip(commands,expected)):
 count=struct.unpack_from('<I',raw)[0]
 x.mem_write(base,raw[4:16]);x.mem_write(base+0x2000,raw[16:] or bytes(24));x.mem_write(base+0x1000,bytes([0xa5])*28)
 x.mem_write(stack,pack(stop,base+0x2000,count,base,base+0x1000))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=bytes(x.mem_read(base+0x1000,28))+pack(x.reg_read(UC_X86_REG_EAX))
 assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_cases,guard_cases=len(commands)-original_cases,scope='Complete original 4a0cb0 with all callees and no hooks. Radius and all six bounds floats match PC/NXDK; all other body/source bytes preserved. Empty and 1..32 random sphere lists, arbitrary centers/positions and signed-zero empty bounds; nonfinite/negative/overflow port guards. Not exhaustive numeric equivalence or live body integration.')
(root/'artifacts/physics-sphere-bounds-verification.json').write_text(json.dumps(report,indent=2));print(report)
