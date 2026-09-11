"""Original force direction and strength block vs PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_force_region_influence\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x486949);commands=bytearray();expected=bytearray()
for case in range(4096):
    flags=case%32
    center=[rng.uniform(-100,100) for _ in range(3)]
    position=[rng.uniform(-100,100) for _ in range(3)]
    matrix=[rng.uniform(-1,1) for _ in range(9)]
    radius_squared=rng.uniform(.01,1000);strength=rng.uniform(-100,100)
    radius=(.25,1.,3.,10.)[case%4];mass=(.125,1.,10.,100.)[(case//32)%4]
    region=w(3,123,flags)+f(*center,*matrix,radius_squared,*([0]*9),strength)+w(1)
    assert len(region)==108
    args=region+f(*position,radius,mass)
    u.mem_write(base,region);u.mem_write(base+0x1000,bytes(0x200))
    u.mem_write(base+0x10e4,f(*position));u.mem_write(base+0x1180,f(radius));u.mem_write(base+0x1098,f(mass))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base);u.reg_write(UC_X86_REG_EBX,base+0x1000)
    u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x486949,0x4869f6,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x4869f6
    result=bytes(u.mem_read(stack+0x1c,12))+bytes(u.mem_read(stack+0x50,4))
    commands.extend(args);expected.extend(result)
    x.mem_write(base,args);x.mem_write(base+0x1000,bytes([0xa5])*16)
    x.mem_write(stack,w(stop,base,base+108)+f(radius,mass)+w(base+0x1000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    actual=bytes(x.mem_read(base+0x1000,16))
    assert actual==result,(case,struct.unpack('<4f',actual),struct.unpack('<4f',result))
    assert bytes(x.mem_read(base,len(args)))==args
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-influence'],input=commands)
assert actual==expected,('PC differs',next(i//16 for i,(a,b) in enumerate(zip(actual,expected)) if a!=b))
report=dict(result='PASS',cases=4096,original_sha256=digest,
 scope='Original prepared486949..4869f6 and vector/normalization/norm callees unchanged. All32 low flag combinations, signed strengths, nonunit directions, unequal physics position/region center, body radius/mass factors below/equal/above one. All16 result bytes exact PC/NXDK; input unchanged. No query, eligibility, rotation, actor application or singularity parity claim.')
(root/'artifacts/force-influence.json').write_text(json.dumps(report,indent=2));print(report)
