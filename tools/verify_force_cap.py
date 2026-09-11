"""Original alternate airborne cap vs PC/NXDK."""
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
entry=int(re.search(r'_rf_physics_force_air_cap\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x486b1c);commands=bytearray();expected=bytearray()
for case in range(2048):
    vx,vz=((0.,0.),(3.,4.),(5.,0.))[case%3] if case<36 else (rng.uniform(-100,100),rng.uniform(-100,100))
    velocity=f(vx,rng.uniform(-1000,1000),vz);speed=(0.,5.,20.,200.)[(case//3)%4]
    flags=rng.getrandbits(32);oldcap=rng.uniform(0,100)
    actor=bytearray(rng.randbytes(0x1500));actor[0x144:0x150]=velocity;actor[0x294:0x298]=w(base+0x2000)
    actor[0x1a8:0x1ac]=w(flags);actor[0x1488:0x148c]=f(oldcap)
    u.mem_write(base,bytes(actor));u.mem_write(base+0x2050,f(speed))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,base+0x144)
    u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x486b1c,0x486b6a,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x486b6a
    result=bytes(u.mem_read(base+0x1488,4))+bytes(u.mem_read(base+0x1a8,4))
    actor[0x1488:0x148c]=result[:4];actor[0x1a8:0x1ac]=result[4:]
    assert bytes(u.mem_read(base,len(actor)))==actor
    args=velocity+f(speed,oldcap)+w(flags);commands.extend(args);expected.extend(result)
    x.mem_write(base,args);x.mem_write(stack,w(stop,base)+f(speed)+w(base+16,base+20))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base+16,8))==result,case
    assert bytes(x.mem_read(base,16))==args[:16]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-cap'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=2048,original_sha256=digest,
 scope='Original486b1c..486b6a and norm/vector callees unchanged; full actor unchanged except alternate cap+1488 and flag200000. Exact PC/NXDK cap/flags, velocity preserved, horizontal speed below/equal/above class cap, differing vertical speed and preexisting flag. No replacement velocity, fall/effect, registration or live integration.')
(root/'artifacts/force-cap.json').write_text(json.dumps(report,indent=2));print(report)
