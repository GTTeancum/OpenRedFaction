"""Full original 4e1630, including direction retries and ordered node traversal."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(raw)+4095)//4096*4096);u.mem_write(0x400000,raw)
base=0x30000000;room=base+0x1000;faces=base+0x2000;vertices=base+0x3000;edges=base+0x4000;nodes=base+0x6000;point=base+0x7000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
u.mem_write(point,f(0,1,0));u.mem_write(stack,w(stop,point)+f(.9753));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x4e0c20,stop,count=10000);initial_direction=struct.unpack('<3f',u.mem_read(point,12))
rng=random.Random(0x4e1630);inputs=[];expected=[];retry_total=0;hits=0
for case in range(600):
 lo=[-2.,-2.,-2.];hi=[2.,2.,2.];pos=[rng.uniform(-3,3) for _ in range(3)]
 if case%4==0:pos=[0.,0.,0.]
 if case%7==0:pos[case%3]=rng.choice([-2.,2.])
 if case%11==0:pos=[a-b for a,b in zip([2.,2.,0.],initial_direction)]
 flags=[(4 if case%19==0 else 0) for _ in range(6)];skip=int(case%23==0);tree=case%2
 inputs.append(f(*pos,*lo,*hi)+w(*flags,skip,tree))
 u.mem_write(base,bytes(0x8000));u.mem_write(point,f(*pos));u.mem_write(base+0x48,f(*lo,*hi));u.mem_write(base+0x9c,w(1,1,base+0x800));u.mem_write(base+0x800,w(room));u.mem_write(room+1,bytes([skip]));u.mem_write(room+0x28,w(faces));u.mem_write(room+0x3c,w(nodes if tree else 0))
 for a in range(6):
  axis=a//2;other=[i for i in range(3) if i!=axis];v=[];plane=[0.,0.,0.,2.];plane[axis]=-1. if a%2 else 1.
  for b,c in [(0,0),(1,0),(1,1),(0,1)]:
   row=[0.,0.,0.];row[axis]=hi[axis] if a%2 else lo[axis];row[other[0]]=hi[other[0]] if b else lo[other[0]];row[other[1]]=hi[other[1]] if c else lo[other[1]];v.append(row)
  flo=lo[:];fhi=hi[:];flo[axis]=fhi[axis]=v[0][axis];face=faces+a*0x100;edge=edges+a*0x100;vert=vertices+a*0x100
  u.mem_write(face,f(*plane,*flo,*fhi)+w(flags[a]));u.mem_write(face+0x40,w(edge,room));u.mem_write(face+0x5c,w(face+0x100 if a<5 else 0));u.mem_write(face+0x58,w(face+0x100 if a%2==0 else 0));u.mem_write(vert,f(*[x for row in v for x in row]))
  for b in range(4):u.mem_write(edge+b*32,w(vert+b*12,0,0,0,0,edge+(b+1)%4*32,edge+(b-1)%4*32))
 for a in range(3):
  n=nodes+a*0x100;u.mem_write(n,f(*lo,*hi)+w(faces+a*0x200));u.mem_write(n+0x20,w(nodes+0x100 if a==0 else 0,nodes+0x200 if a==0 else 0))
 u.mem_write(stack,w(stop,base,point));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4e1630,stop,count=200000);assert u.reg_read(UC_X86_REG_EIP)==stop
 owner=u.reg_read(UC_X86_REG_EAX);assert owner in (0,room)
 selected=struct.unpack('<I',u.mem_read(stack-0x1014,4))[0];retries=struct.unpack('<I',u.mem_read(stack-0x1054,4))[0]
 retry_total+=retries;hits+=bool(owner);expected.append(w(0,0 if owner else 0xffffffff,(selected-faces)//0x100 if owner else 0xffffffff,retries))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_collision_probe.exe'),'--locate-room'],input=b''.join(inputs))
for i,e in enumerate(expected):assert actual[i*16:(i+1)*16]==e,('PC mismatch',i,actual[i*16:(i+1)*16].hex(),e.hex())
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_collision_locate_room\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
for case,wire in enumerate(inputs):
 pos=struct.unpack('<3f',wire[:12]);lo=struct.unpack('<3f',wire[12:24]);hi=struct.unpack('<3f',wire[24:36]);flags=struct.unpack('<6I',wire[36:60]);skip,tree=struct.unpack('<2I',wire[60:])
 x.mem_write(base,bytes(0xa000));x.mem_write(point,wire[:12]);x.mem_write(base+0x8000,wire[12:36]);x.mem_write(base+0x900c,w(0))
 x.mem_write(room,f(*lo,*hi)+w(skip,0,0,base+0x100))
 x.mem_write(base+0x100,w(0,nodes if tree else 0,faces,0,base+0x6100,3 if tree else 0,6,3 if tree else 0,0,0))
 for a in range(6):
  axis=a//2;other=[i for i in range(3) if i!=axis];v=[];plane=[0.,0.,0.,2.];plane[axis]=-1. if a%2 else 1.
  for b,c in [(0,0),(1,0),(1,1),(0,1)]:
   row=[0.,0.,0.];row[axis]=hi[axis] if a%2 else lo[axis];row[other[0]]=hi[other[0]] if b else lo[other[0]];row[other[1]]=hi[other[1]] if c else lo[other[1]];v.append(row)
  flo=list(lo);fhi=list(hi);flo[axis]=fhi[axis]=v[0][axis];vert=vertices+a*0x100
  x.mem_write(faces+a*72,f(*plane,*flo,*fhi)+w(vert,4,0,flags[a],0,0,0,0));x.mem_write(vert,f(*[z for row in v for z in row]))
 for a in range(3):x.mem_write(nodes+a*40,f(*lo,*hi)+w(a*2,2,1 if a==0 else 0xffffffff,2 if a==0 else 0xffffffff))
 x.mem_write(stack,w(stop,room,1,base+0x900c,1,base+0x8000,base+0x800c,point,base+0x9000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x9000,12));assert got==expected[case],('NXDK mismatch',case,got.hex(),expected[case].hex())
report=dict(result='PASS',original_cases=len(inputs),nxdk_cases=len(inputs),hits=hits,retries=retry_total,scope='Complete original 4e1630 and all callees. Synthetic cube, flat ordered faces and three-node trees, room skips, face flags, inside/outside/boundary points. Real level room ownership and XEMU runtime not yet checked.')
(ROOT/'artifacts/room-locator-verification.json').write_text(json.dumps(report,indent=2));print(report)
