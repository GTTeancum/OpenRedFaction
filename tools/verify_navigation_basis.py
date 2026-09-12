"""Full4fcea0 direction basis versus shared PC/compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;node=base+0x100;out=base+0x200;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_basis\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4fcea0);commands=[];expected=[];vertical=0
edge=struct.unpack('<I',f(.0001))[0];thresholds=[struct.unpack('<f',w(edge+d))[0] for d in (-1,0,1)]
for case in range(4096):
 direction=[rng.uniform(-100,100) for _ in range(3)]
 if case<1024:direction=[rng.choice([0]+thresholds)*rng.choice((-1,1)),rng.choice((-1,1)),rng.choice([0]+thresholds)*rng.choice((-1,1))]
 wire=f(*direction);u.mem_write(base,wire);u.mem_write(out,bytes([0xa5])*36);u.reg_write(UC_X86_REG_ECX,out)
 call(u,0x4fcea0,[base]);want=bytes(u.mem_read(out,36));assert bytes(u.mem_read(base,12))==wire
 if want[24:28]==w(0) and want[32:36]==w(0):vertical+=1
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*36)
 assert call(x,entry,[base,out])==0
 assert bytes(x.mem_read(out,36))==want,(case,direction,want.hex(),bytes(x.mem_read(out,36)).hex())
 commands.append(wire);expected.append(w(0)+want)
for wire in (f(0,0,0),f(float('nan'),1,0),f(1,float('inf'),0),f(1,0,float('-inf'))):
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*36);status=call(x,entry,[base,out]);assert status!=0 and bytes(x.mem_read(out,36))==bytes([0xa5])*36
 commands.append(wire);expected.append(w(status)+bytes([0xa5])*36)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-basis'],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
# Full pair with coincident centers: static vector constructors already complete.
u.mem_write(0x1754474,b'\x07')
for point in ([0,0,0],[1,2,3],[100,-100,100]):
 for same in (0,1):
  u.mem_write(base,f(*point));u.mem_write(base+0x100,bytes(0x100))
  u.mem_write(base+0x11c,f(2,4));u.mem_write(base+0x15c,f(2,4));u.mem_write(out,w(0x12345678))
  assert call(u,0x40b3d0,[base,0x3f800000,base+0x100,base+0x100 if same else base+0x140,out])==2
  assert bytes(u.mem_read(out,4))==w(0x12345678)
report=dict(result='PASS',cases=4096,vertical_cases=vertical,guards=4,original_coincident_pair_cases=6,scope='Full original4fcea0 with actual normalization and cross product callees, no hooks,53-bit x87. Exact nine matrix words vs PC/compiled NXDK for finite nonzero directions, strict vertical thresholds and signed directions. Zero/nonfinite direction rejects preserving output in port. Coincident navigation pair behavior still needs separate original-path handling; this does not complete pair geometry or scene navigation.')
(root/'artifacts/navigation-basis-verification.json').write_text(json.dumps(report,indent=2));print(report)
