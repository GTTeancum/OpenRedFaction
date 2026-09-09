"""Compare complete original swept sphere/plane contact and port guards."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);out=base+4096;stack=base+60000;stop=base+64000
rng=random.Random(0x5071b0);cases=[];expected=[];hits=overlaps=0
for n in range(9000):
 normal=[rng.choice([-1,0,1]) for _ in range(3)];start=[rng.uniform(-8,8) for _ in range(3)];delta=[rng.uniform(-16,16) for _ in range(3)];radius=rng.choice([0,.001,.25,1,2,8]);d=rng.uniform(-3,3)
 if n%3==1:normal=[rng.uniform(-1,1) for _ in range(3)]
 if n%3==0:
  normal=[0,0,1];d=0;start[2]=rng.choice([-1,0,radius,radius+.001,radius+1]);delta[2]=rng.choice([0,1,-.001,-1,-16])
 wire=struct.pack('<11f',*start,*delta,radius,*normal,d);cases.append(wire);u.mem_write(base,wire);u.mem_write(out,bytes([0xa5])*16)
 u.mem_write(stack,struct.pack('<7I',stop,base,base+12,struct.unpack_from('<I',wire,24)[0],base+28,out,out+4));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x5071b0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit;overlaps+=bool(hit and struct.unpack('<f',u.mem_read(out,4))[0]==0)
 expected.append(struct.pack('<iI',0,hit)+bytes(u.mem_read(out,16)))
for offset,value in [(24,-1),(24,math.nan),(0,math.inf),(28,math.nan)]:
 wire=bytearray(cases[0]);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*20)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--sphere-plane'],input=b''.join(cases));assert len(raw)==len(cases)*24
for n,want in enumerate(expected):assert raw[n*24:(n+1)*24]==want,('PC',n,raw[n*24:(n+1)*24].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_sphere_plane\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*20)
 x.mem_write(stack,struct.pack('<8I',stop,base,base+12,struct.unpack_from('<I',wire,24)[0],base+28,out+4,out+8,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,20));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=9000,port_guards=4,hits=hits,zero_fraction_hits=overlaps,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Complete unmodified original 5071b0 and vector helpers; exact fraction/contact output including misses. Axial boundary/overlap and randomized planes, radius-zero and approach-direction checks. PC and NXDK-linked CPU comparison; no polygon edges or actor movement.')
(root/'artifacts/sphere-plane-verification.json').write_text(json.dumps(report,indent=2));print(report)
