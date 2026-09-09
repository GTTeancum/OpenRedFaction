"""Compare original moving-solid contact output conversion."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);out=base+4096;stack=base+60000;stop=base+64000
rng=random.Random(0x498fb4);cases=[];expected=[]
for n in range(12000):
 point=[rng.uniform(-100,100) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)];origin=[rng.uniform(-100,100) for _ in range(3)];fraction=rng.random()
 angle=rng.uniform(-math.pi,math.pi);c,s=math.cos(angle),math.sin(angle);matrix=[c,0,s,0,1,0,-s,0,c]
 if n%3==0:matrix=[rng.uniform(-1,1) for _ in range(9)]
 if n%7==0:origin=[v*10000000 for v in origin]
 if n%11==0:point=[v*1e-30 for v in point]
 wire=struct.pack('<19f',fraction,*point,*normal,*origin,*matrix);cases.append(wire)
 u.mem_write(base+0xe4,wire[28:40]);u.mem_write(base+0xfc,wire[40:76]);u.mem_write(out,bytes([0xa5])*28)
 u.mem_write(stack+0x18,wire[:28]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base);u.reg_write(UC_X86_REG_ESI,out);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x498fb4,0x499011,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x499011
 result=bytes(u.mem_read(out,28));expected.append(struct.pack('<i',0)+result[24:28]+result[:24])
original_count=len(cases)
for offset,value in [(0,math.nan),(4,math.inf),(16,math.nan),(28,math.inf),(40,math.nan)]:
 wire=bytearray(cases[1]);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*28)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--contact-world'],input=b''.join(cases));assert len(raw)==len(cases)*32
for n,want in enumerate(expected):assert raw[n*32:(n+1)*32]==want,('PC',n,raw[n*32:(n+1)*32].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_contact_world\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*28)
 x.mem_write(stack,struct.pack('<5I',stop,base,base+28,base+40,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,28));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_count,port_guards=5,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Original 498fb4..499011 geometric output block, all vector/matrix helpers unchanged. Exact fraction, point and normal, PC and NXDK. Output pose distinct from query input pose; no material lookup, object metadata or moving-solid iteration.')
(root/'artifacts/collision-contact-world-verification.json').write_text(json.dumps(report,indent=2));print(report)
