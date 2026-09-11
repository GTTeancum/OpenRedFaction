"""Original force-region alternate airborne cap, prepared 486b1c..486b6a."""
import hashlib,json,math,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));blob=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(0x400000,(len(blob)+4095)//4096*4096);m.mem_write(0x400000,blob)
base=0x30000000;stack=base+0xe000;m.mem_map(base,65536)
w=lambda v:struct.pack('<I',v);f=lambda v:struct.pack('<f',v)
assert bytes(m.mem_read(0x5893f8,4))==f(1)
rng=random.Random(0x486b1c);raised=0
for i in range(512):
    vx,vz=((0.,0.),(3.,4.),(5.,0.))[i%3] if i<12 else (rng.uniform(-20,20),rng.uniform(-20,20))
    velocity=struct.pack('<3f',vx,rng.uniform(-100,100),vz);vx,_,vz=struct.unpack('<3f',velocity)
    speed=(0.,5.,20.)[i%3];flags=rng.getrandbits(32)
    actor=bytearray(rng.randbytes(0x1500));actor[0x144:0x150]=velocity
    actor[0x294:0x298]=w(base+0x2000);actor[0x1a8:0x1ac]=w(flags)
    m.mem_write(base,bytes(actor));m.mem_write(base+0x2050,f(speed))
    m.reg_write(UC_X86_REG_ESI,base);m.reg_write(UC_X86_REG_EDI,base+0x144)
    m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x37f)
    m.emu_start(0x486b1c,0x486b6a,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==0x486b6a
    norm=math.sqrt(vx*vx+vz*vz);cap=norm+1 if norm>speed else speed
    raised+=norm>speed;actor[0x1488:0x148c]=f(cap);actor[0x1a8:0x1ac]=w(flags|0x200000)
    assert bytes(m.mem_read(base,len(actor)))==actor,i
report=dict(result='PASS',cases=512,raised_caps=raised,original_sha256=digest,
    scope='Unchanged original prepared cap block and norm/vector callees. Ignores vertical velocity; class speed when horizontal norm is not greater, else norm+1; sets flag200000 and preserves other bytes. No force-region query, force application or C/NXDK ownership integration.')
(root/'artifacts/air-speed-cap.json').write_text(json.dumps(report,indent=2));print(report)
