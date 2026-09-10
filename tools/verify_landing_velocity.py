"""Original landing support/contact velocity rebasing with actual vector callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
source=root/'Installed_Game/RF.exe';assert hashlib.sha256(source.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(source);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_landing_velocity\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x419901);commands=bytearray();expected=bytearray()
def nxdk(wire,want,alias=False):
 x.mem_write(b,wire);out=b if alias else b+0x1000
 if not alias:x.mem_write(out,bytes([0xa5])*12)
 x.mem_write(stack,w(stop,b,b+12,b+24,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,12));assert got==want,('NXDK',got.hex(),want.hex())
for n in range(4096):
 values=[rng.uniform(-100,100)*(1e7 if n%5==0 else 1) for _ in range(9)]
 if n%7==0:values[6:9]=values[3:6]
 if n%11==0:values[3:9]=[0]*6
 wire=struct.pack('<9f',*values);commands.extend(wire)
 u.mem_write(b+0x144,wire[:12]);u.mem_write(b+0x8a0,wire[12:24]);u.mem_write(b+0x1d8,wire[24:36]);u.mem_write(stack,bytes(64))
 u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x419901,0x41993a,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x41993a
 want=w(0)+bytes(u.mem_read(b+0x144,12));expected.extend(want);nxdk(wire,want,alias=n%2==0)
for i in range(9):
 wire=bytearray(36);wire[i*4:i*4+4]=w(0x7fc00000);wire=bytes(wire)
 want=w(-2)+bytes([0xa5])*12;commands.extend(wire);expected.extend(want);nxdk(wire,want)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--landing-velocity'],input=commands)
assert actual==expected,'PC landing velocity differs'
report=dict(result='PASS',cases=4096,guards=9,scope='Original419901..41993a and unchanged vector callees vs PC/NXDK stored velocity rebasing, including large magnitudes, cancellation and NXDK aliasing. Does not include subsequent class dispatch, vertical reset, landing sounds or scene wiring.')
(root/'artifacts/landing-velocity-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
