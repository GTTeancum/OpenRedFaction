"""Compare original solid-local collision input preparation."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);out=base+4096;stack=base+60000;stop=base+64000
rng=random.Random(0x4df25c);cases=[];expected=[];inactive=collapsed=0
for n in range(12000):
 start=[rng.uniform(-100,100) for _ in range(3)];delta=[rng.uniform(-8,8) for _ in range(3)];origin=[rng.uniform(-100,100) for _ in range(3)]
 angle=rng.uniform(-math.pi,math.pi);c,s=math.cos(angle),math.sin(angle);matrix=[c,0,s,0,1,0,-s,0,c]
 if n%3==0:matrix=[rng.uniform(-1,1) for _ in range(9)]
 if n%7==0:start=[v*10000000 for v in start]
 if n%11==0:delta=[v*1e-30 for v in delta]
 if n%17==0:delta=[0,-0.0,0]
 flags=rng.choice([0,1,4,0x464]);wire=struct.pack('<18fI',*start,*delta,*origin,*matrix,flags);cases.append(wire)
 # Original query layout: origin +4, matrix +10, start +34, delta +40.
 u.mem_write(base,bytes(128));u.mem_write(base+4,wire[24:72]);u.mem_write(base+0x34,wire[:24]);u.mem_write(base+0x50,struct.pack('<I',flags));u.mem_write(base+0x54,bytes([0xa5])*24)
 u.mem_write(out,bytes(40));u.mem_write(stack,struct.pack('<4I',stop,base,out,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base+8192);u.reg_write(UC_X86_REG_FPCW,0x37f)
 active=int(any(delta));inactive+=not active
 boundary=0x4df302 if active else 0x4df681
 u.emu_start(0x4df1c0,boundary,count=100000);assert u.reg_read(UC_X86_REG_EIP)==boundary
 result=bytes(u.mem_read(base+0x54,24));collapsed+=bool(active and not any(struct.unpack('<3f',result[12:])))
 expected.append(struct.pack('<iI',0,active)+result)
original_count=len(cases)
for offset,value in [(0,math.nan),(12,math.inf),(24,math.nan),(36,math.inf)]:
 wire=bytearray(cases[1]);struct.pack_into('<I',wire,72,0);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*28)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--query-local'],input=b''.join(cases));assert len(raw)==len(cases)*32
for n,want in enumerate(expected):assert raw[n*32:(n+1)*32]==want,('PC',n,raw[n*32:(n+1)*32].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_query_local\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*28)
 x.mem_write(stack,struct.pack('<9I',stop,base,base+12,base+24,base+36,struct.unpack_from('<I',wire,72)[0],out+4,out+16,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,28));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_count,port_guards=4,inactive=inactive,collapsed_transformed_displacements=collapsed,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Original 4df1c0 entry through 4df302 input preparation or 4df681 zero-motion exit; all vector and matrix callees unchanged. Exact local vectors and zero-motion preservation, PC and NXDK. No contact/world-output transformation or moving-solid iteration.')
(root/'artifacts/collision-query-local-verification.json').write_text(json.dumps(report,indent=2));print(report)
