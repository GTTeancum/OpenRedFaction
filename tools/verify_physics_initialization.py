"""Original physics preparation/initialization with no collision spheres."""
import hashlib,json,struct,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;params=base+0x2000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
matrix=floats(1,0,0,0,1,0,0,0,1);cases=0
for material in (0,1,9,10,0xffffffff):
 for flags in (0,1,0x80000000,0x20000):
  for mass in (1.0,2.0,8.0):
   for position in ((0,0,0),(4,-8,16)):
    p=bytearray(0xa0);p[0xc:0x10]=floats(.25);p[0x10:0x14]=pack(material);p[0x14:0x18]=floats(mass)
    p[0x18:0x3c]=matrix;p[0x3c:0x48]=floats(*position);p[0x48:0x6c]=matrix
    p[0x6c:0x78]=floats(1,2,3);p[0x78:0x84]=floats(2,3,4);p[0x84:0x88]=floats(99);p[0x94:0x98]=pack(flags)
    u.mem_write(params,bytes(p));u.mem_write(base,bytes([0xa5])*0x170);u.mem_write(base+0xfc,bytes(12))
    u.mem_write(stack,pack(stop,base,params));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49ec90,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    actual=bytes(u.mem_read(base,0x170));selected=material if 0<material<10 else 0
    assert actual[:4]==bytes(u.mem_read(0x649f50+28*selected,4))
    assert actual[8:12]==bytes(u.mem_read(0x649f54+28*selected,4))
    assert actual[4:8]==floats(.25) and actual[0x10:0x14]==floats(mass)
    for offset in (0x5c,0x68):assert actual[offset:offset+12]==floats(*position)
    for offset in (0x14,0x38,0x74,0x98):assert actual[offset:offset+36]==matrix,(hex(offset),actual[offset:offset+36].hex())
    assert actual[0xbc:0xc8]==floats(1,2,3) and actual[0xc8:0xd4]==floats(2,3,4)
    assert actual[0xd4:0xe0]==floats(2*mass,3*mass,4*mass)
    assert actual[0xf8:0xfc]==floats(0) and actual[0xfc:0x108]==bytes(12)
    assert actual[0x120:0x128]==pack(flags,0)
    assert actual[0x138:0x148]==floats(0,1,0,1)
    assert actual[0x15c:0x160]==pack(0xffffffff) and actual[0x164:0x16c]==bytes(8)
    assert bytes(u.mem_read(params,len(p)))==p
    cases+=1

# Verify every destination byte with nonidentity tensors/orientations and the
# direct initializer's preserve-state byte. Dyadic inputs make the independent
# matrix arithmetic exact before each documented float store.
rng=random.Random(0x49f010);full_cases=0
f32=lambda v:struct.unpack('<f',floats(v))[0]
def multiply(a,b):
 return [f32(sum(a[row*3+k]*b[k*3+col] for k in range(3))) for row in range(3) for col in range(3)]
for case in range(360):
 material=(0,1,9,10,0xffffffff)[case%5];selected=material if 0<material<10 else 0
 for index in range(10):u.mem_write(0x649f50+28*index,floats((index+1)*.25,(index+1)*.5,(index+1)*.75))
 flags=(0,1,0x80000000,0x20000)[case%4];mass=(1,2,8)[case%3]
 tensor=[rng.randrange(-16,17)*.125 for _ in range(9)]
 orientation=[rng.randrange(-8,9)*.25 for _ in range(9)]
 position=[rng.randrange(-256,257)*.125 for _ in range(3)]
 velocity=[rng.randrange(-32,33)*.125 for _ in range(3)];vector=[rng.randrange(-32,33)*.125 for _ in range(3)]
 p=bytearray(0xa0);p[0xc:0x10]=floats(.25);p[0x10:0x14]=pack(material);p[0x14:0x18]=floats(mass)
 p[0x18:0x3c]=floats(*tensor);p[0x3c:0x48]=floats(*position);p[0x48:0x6c]=floats(*orientation)
 p[0x6c:0x78]=floats(*velocity);p[0x78:0x84]=floats(*vector);p[0x84:0x88]=floats(99);p[0x94:0x98]=pack(flags)
 fill=(0,0x5a,0xa5)[(case//3)%3];preserve=(0,1,255)[case%3]
 seed=bytearray([fill]*0x170);seed[0xfc:0x108]=bytes(12)
 u.mem_write(params,bytes(p));u.mem_write(base,bytes(seed));u.mem_write(stack,pack(stop,base,params,preserve))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49f010,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 want=bytearray(seed);want[0:12]=floats((selected+1)*.25,.25,(selected+1)*.5);want[0x10:0x14]=floats(mass)
 want[0x14:0x38]=floats(*tensor)
 transpose=[orientation[col*3+row] for row in range(3) for col in range(3)]
 world=multiply(transpose,multiply(tensor,orientation));want[0x38:0x5c]=floats(*world)
 for offset in (0x5c,0x68,0x108,0x114):want[offset:offset+12]=floats(*position)
 for offset in (0x74,0x98):want[offset:offset+36]=floats(*orientation)
 want[0xbc:0xc8]=floats(*velocity);want[0xc8:0xd4]=floats(*vector);want[0xd4:0xe0]=floats(*(v*mass for v in vector))
 want[0xe0:0xf8]=bytes(24);want[0xf8:0xfc]=floats(0);want[0x120:0x124]=pack(flags)
 if not preserve:want[0x124:0x128]=pack(0)
 want[0x138:0x148]=floats(0,1,0,1);want[0x15c:0x160]=pack(0xffffffff);want[0x164:0x16c]=bytes(8)
 actual=bytes(u.mem_read(base,len(seed)))
 assert actual==want,('full body',case,[(hex(i),actual[i:i+4].hex(),want[i:i+4].hex()) for i in range(0,len(seed),4) if actual[i:i+4]!=want[i:i+4]])
 assert bytes(u.mem_read(params,len(p)))==p
 full_cases+=1
report=dict(result='PASS',preparation_cases=cases,full_initializer_cases=full_cases,scope='Complete original 49ec90/49f010 and all callees with no collision-sphere mode and empty destination list; no hooks. Prior 120 identity cases plus 360 complete destination-byte comparisons for dyadic nonsymmetric tensors/orientations, seeded materials, velocities, mass products, zero-radius bounds, preserved fields, patterned storage and direct preserve-state byte. Parameters unchanged. No sphere allocation, general float precision proof or shared body initializer.')
(root/'artifacts/physics-initialization-verification.json').write_text(json.dumps(report,indent=2));print(report)
