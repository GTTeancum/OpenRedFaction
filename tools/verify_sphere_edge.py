"""Compare complete original swept sphere/edge contact and port guards."""
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
# Static vectors already initialized, as on every call after the first. Avoid
# unrelated CRT atexit registration; no query helper is replaced or skipped.
u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
a=(0,0,0);b=(0,2,0);endpoint_time=(2-math.sqrt(.1875))/4
fixtures=[
 ('edge interior',(-2,1,0),(4,0,0),.5,a,b,1,.375,(0,1,0)),
 ('exact tangent',(-2,1,.5),(4,0,0),.5,a,b,1,None,None),
 ('start endpoint',(-2,-.25,0),(4,0,0),.5,a,b,1,endpoint_time,a),
 ('far endpoint omitted',(-2,2.25,0),(4,0,0),.5,a,b,1,None,None),
 ('reversed endpoint',(-2,2.25,0),(4,0,0),.5,b,a,1,endpoint_time,b),
 ('equal limit rejected',(-2,1,0),(4,0,0),.5,a,b,.375,None,None),
 ('larger limit accepted',(-2,1,0),(4,0,0),.5,a,b,.3750001,.375,(0,1,0)),
 ('small negative entry',(-.49,1,0),(1,0,0),.5,a,b,1,1e-6,(0,1,0)),
 ('deep initial overlap',(0,1,0),(1,0,0),.5,a,b,1,None,None),
 ('parallel endpoint',(0,-2,0),(0,4,0),.5,a,b,1,.375,a),
 ('zero movement',(-2,1,0),(0,0,0),.5,a,b,1,None,None),
 ('zero edge',(-2,0,0),(4,0,0),.5,a,a,1,.375,a),
 ('zero radius',(-2,1,0),(4,0,0),0,a,b,1,None,None),
]
rng=random.Random(0x5072e0);cases=[];expected=[];hits=0
for fixture in fixtures:
 _,start,delta,radius,a,b,limit,_,_=fixture
 cases.append(struct.pack('<14f',*start,*delta,radius,*a,*b,limit))
for n in range(9000):
 a=[rng.uniform(-8,8) for _ in range(3)];b=[v+rng.uniform(-4,4) for v in a]
 start=[v+rng.uniform(-3,3) for v in a];delta=[rng.uniform(-8,8) for _ in range(3)];radius=rng.choice([0,.001,.25,.5,1,2]);limit=rng.choice([0,.1,.5,1])
 if n%3==0:
  a=[0,0,0];b=[0,2,0];start=[rng.uniform(-2,2),rng.choice([0,2,-.25,2.25,1]),rng.choice([0,radius])];delta=[rng.uniform(-8,8),0,0]
 if n%31==0:b=a[:]
 if n%37==0:delta=[0,0,0]
 cases.append(struct.pack('<14f',*start,*delta,radius,*a,*b,limit))
for wire in cases:
 u.mem_write(base,wire);u.mem_write(out,bytes([0xa5])*16)
 u.mem_write(stack,struct.pack('<9I',stop,out+4,base,base+12,struct.unpack_from('<I',wire,24)[0],base+28,base+40,out,struct.unpack_from('<I',wire,52)[0]));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x5072e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit
 expected.append(struct.pack('<iI',0,hit)+bytes(u.mem_read(out,16)))
original_count=len(cases)
for offset,value in [(24,-1),(24,math.nan),(0,math.inf),(28,math.nan),(52,-1),(52,2),(52,math.nan)]:
 wire=bytearray(cases[0]);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*20)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--sphere-edge'],input=b''.join(cases));assert len(raw)==len(cases)*24
for n,want in enumerate(expected):assert raw[n*24:(n+1)*24]==want,('PC',n,raw[n*24:(n+1)*24].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_sphere_edge\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);x.mem_write(out,bytes([0xa5])*20)
 x.mem_write(stack,struct.pack('<10I',stop,base,base+12,struct.unpack_from('<I',wire,24)[0],base+28,base+40,struct.unpack_from('<I',wire,52)[0],out+4,out+8,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,20));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_count,port_guards=7,hits=hits,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Complete original steady-state 5072e0, geometric callees unchanged; exact hit, fraction, contact and preserved miss outputs on PC and NXDK. Includes 13 analytic boundaries and randomized edges. No finite-face composition or actor movement.')
(root/'artifacts/sphere-edge-verification.json').write_text(json.dumps(report,indent=2));print(report)
