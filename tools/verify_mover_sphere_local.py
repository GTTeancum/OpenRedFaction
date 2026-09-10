"""Original physics mover-sphere endpoint preparation, including float stores."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_collision_mover_sphere_local\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x499fef);commands=bytearray();expected=bytearray()
for n in range(4096):
 center=[rng.uniform(-2,2) for _ in range(3)];body=[rng.uniform(-1,1) for _ in range(9)]
 start=[rng.uniform(-100,100)*(1e7 if n%5==0 else 1) for _ in range(3)];end=[v+rng.uniform(-5,5) for v in start]
 origin=[rng.uniform(-100,100) for _ in range(3)];matrix=[rng.uniform(-1,1) for _ in range(9)]
 if n%7==0:end=start[:]
 wire=struct.pack('<30f',*center,*body,*start,*end,*origin,*matrix);commands.extend(wire)
 u.mem_write(base+0x1000,wire[48:60]);u.mem_write(base+0x1010,wire[60:72]);u.mem_write(base+0x2000,bytes(0x200));u.mem_write(base+0x2074,wire[12:48]);u.mem_write(base+0x20fc,w(1,1,base+0x4000));u.mem_write(base+0x4000,wire[:12]+bytes(12))
 u.mem_write(base+0x30e4,wire[72:84]);u.mem_write(base+0x3048,wire[84:120]);u.mem_write(stack,bytes(0x300));u.mem_write(stack+0x168,w(base+0x1010,base+0x2000))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base+0x3048);u.reg_write(UC_X86_REG_EBP,base+0x3000);u.reg_write(UC_X86_REG_EDI,base+0x1000);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x499fef,0x49a0c9,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49a0c9
 want=w(0)+bytes(u.mem_read(stack+0xec,24));expected.extend(want)
 x.mem_write(base,wire);x.mem_write(base+0x5000,bytes([0xa5])*24);x.mem_write(stack,w(stop,base,base+12,base+48,base+60,base+72,base+84,base+0x5000,base+0x500c));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x5000,24));assert got==want,('NXDK',n,got.hex(),want.hex())
for index in range(30):
 guard=bytearray(wire);guard[index*4:index*4+4]=w(0x7fc00000)
 want=w(-2)+bytes([0xa5])*24;commands.extend(guard);expected.extend(want)
 x.mem_write(base,bytes(guard));x.mem_write(base+0x5000,bytes([0xa5])*24);x.mem_write(stack,w(stop,base,base+12,base+48,base+60,base+72,base+84,base+0x5000,base+0x500c));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x5000,24))==want
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-sphere-local'],input=commands);assert actual==expected
report=dict(result='PASS',cases=4096,port_guards=30,original_sha256=digest,scope='Original 499fef..49a0c9 with unchanged sphere-array and vector/matrix callees, no hooks. Original64-bit x87 arithmetic, port explicit dot precision; NXDK caller53-bit control. Exact PC/NXDK local endpoints/delta including large coordinates and zero displacement. Query flags, broad phase, contact selection/response and live player wiring excluded.')
(root/'artifacts/mover-sphere-local-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
