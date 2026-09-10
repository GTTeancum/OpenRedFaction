"""Original mover-hit output conversion and response metadata."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_collision_mover_contact\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def surface(m,a,s,c):
 sp=m.reg_read(UC_X86_REG_ESP);ret,actual=struct.unpack('<2I',m.mem_read(sp,8));assert actual==texture
 m.reg_write(UC_X86_REG_EAX,material);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,surface,begin=0x468700,end=0x468700)
rng=random.Random(0x49a118);commands=bytearray();expected=bytearray()
for n in range(2048):
 fraction=rng.random();point=[rng.uniform(-100,100) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)]
 origin=[rng.uniform(-100,100) for _ in range(3)];matrix=[rng.uniform(-1,1) for _ in range(9)];velocity=[rng.uniform(-10,10) for _ in range(3)]
 obj,texture,material,flags,face=[rng.getrandbits(32) for _ in range(5)]
 wire=struct.pack('<22f5I',fraction,*point,*normal,*origin,*matrix,*velocity,obj,texture,material,flags,face);commands.extend(wire)
 u.mem_write(base+0x1000,bytes(0x400));u.mem_write(base+0x1048,wire[40:76]);u.mem_write(base+0x10e4,wire[28:40]);u.mem_write(base+0x1144,wire[76:88]);u.mem_write(base+0x102c,w(obj));u.mem_write(base+0x2000,bytes(0x60));u.mem_write(base+0x2028,w(flags));u.mem_write(base+0x2030,w(texture));u.mem_write(base+0x3000,bytes([0xa5])*68)
 u.mem_write(stack,bytes(0x300));u.mem_write(stack+0x34,wire[:28]);u.mem_write(stack+0x54,w(base+0x2000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base+0x1048);u.reg_write(UC_X86_REG_EBP,base+0x1000);u.reg_write(UC_X86_REG_ESI,base+0x3000);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49a118,0x49a1e1,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49a1e1
 value=bytearray(u.mem_read(base+0x3000,68));assert value[60:64]==w(base+0x2000);value[60:64]=w(face);want=w(0)+value;expected.extend(want)
 x.mem_write(base,wire);x.mem_write(base+0x5000,bytes([0xa5])*68);x.mem_write(stack,w(stop,base,base+28,base+40,base+76,obj,texture,material,flags,face,base+0x5000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x5000,68));assert got==want,('NXDK',n,got.hex(),want.hex())
for index in range(22):
 guard=bytearray(wire);guard[index*4:index*4+4]=w(0x7fc00000);want=w(-2)+bytes([0xa5])*68;commands.extend(guard);expected.extend(want)
 x.mem_write(base,bytes(guard));x.mem_write(base+0x5000,bytes([0xa5])*68);x.mem_write(stack,w(stop,base,base+28,base+40,base+76,obj,texture,material,flags,face,base+0x5000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x5000,68))==want
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-contact'],input=commands);assert actual==expected
report=dict(result='PASS',cases=2048,port_guards=22,original_sha256=digest,scope='Original 49a118..49a1df with actual vector transforms and face flag helper; only material468700 lookup supplied. Exact PC/NXDK point/normal/fraction, velocity, object/texture/material/face metadata and zero fields; face pointer remapped to token. Original64-bit x87, NXDK caller53-bit with explicit dot precision. Selection, materials loading, physical response and live mover collision excluded.')
(root/'artifacts/mover-contact-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
