"""Compare complete original finite-face sweeps with PC and NXDK."""
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
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

rng=random.Random(0x4dec10);cases=[];expected=[];hits=0;edge_hits=0;multiple_hits=0
for n in range(12000):
 axis=n%3;sign=rng.choice([-1,1]);height=rng.uniform(-10,10);normal=[0,0,0];normal[axis]=sign
 plane=normal+[-sign*height];vertices=[]
 for a,b in [(-4,-3),(4,-3),(4,3),(-4,3)]:
  # Normal axis is explicit; other coordinates use cyclic axes.
  v=[0,0,0];v[axis]=height;v[(axis+1)%3]=a;v[(axis+2)%3]=b;vertices.append(v)
 if n%2:vertices.reverse()
 lo=[min(v[j] for v in vertices) for j in range(3)];hi=[max(v[j] for v in vertices) for j in range(3)]
 start=[rng.uniform(-8,8) for _ in range(3)];start[axis]=height+sign*rng.uniform(.1,8)
 delta=[rng.uniform(-12,12) for _ in range(3)];delta[axis]=-sign*rng.uniform(.1,16)
 if n%13==0:delta[axis]*=-1
 if n>=6000:
  # Rotate the complete fixture to exercise non-axial planes and normals.
  angle=rng.uniform(-math.pi,math.pi);tilt=rng.uniform(-math.pi,math.pi)
  ca,sa,ct,st=math.cos(angle),math.sin(angle),math.cos(tilt),math.sin(tilt)
  def rotate(v):
   x=ca*v[0]-sa*v[1];y=sa*v[0]+ca*v[1];z=v[2]
   return [ct*x+st*z,y,-st*x+ct*z]
  normal=rotate(normal);plane=normal+[plane[3]];vertices=[rotate(v) for v in vertices]
  start=rotate(start);delta=rotate(delta)
  lo=[min(v[j] for v in vertices) for j in range(3)];hi=[max(v[j] for v in vertices) for j in range(3)]
 radius=rng.choice([0,.00001,.0001,.25,.5,1,2]);normal_delta=delta[:]
 if n%5==0:normal_delta=[v+rng.uniform(-1,1) for v in delta]
 limit=rng.choice([.25,.5,1]);flags=rng.choice([0,0,0,1,4,0x40,0x80,0x2000]);prop=rng.choice([-1,0,0,1]);present=rng.randrange(2);kind=rng.choice([0,1,2]);state=rng.choice([0,1])
 if n<6:
  normal=[0,0,1];plane=[0,0,1,0];vertices=[[-1,-1,0],[1,-1,0],[1,1,0],[-1,1,0]]
  lo=[-1,-1,0];hi=[1,1,0];start=[0 if n==0 else 1.25,0,2];delta=[0,0,-4];radius=.5;limit=1
  flags=prop=present=kind=state=0
  if n in (0,2):limit=.375
  if n==3:limit=.25
  if n==4:delta=[0,0,4]
  if n==5:start=[1.5,0,2] # Exact tangent at the nearest edge.
  normal_delta=delta[:]
 filter=[0x461,flags,prop,present,kind,state]
 wire=struct.pack('<41fIIiIIII',*plane,*lo,*hi,*[v for vert in vertices for v in vert],*([0]*12),*start,*delta,limit,*filter,4);wire+=struct.pack('<4f',*normal_delta,radius);cases.append(wire)
 # Round source data to the binary32 wire representation before mapping original views.
 face=base;query=base+4096;owner=base+8192;edges=base+12288;out=base+16384;verts=base+20480
 u.mem_write(face,wire[:16]);u.mem_write(face+0x10,wire[16:40]);u.mem_write(face+0x28,struct.pack('<I',flags));u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x34,struct.pack('<h',prop));u.mem_write(face+0x40,struct.pack('<II',edges,owner if present else 0))
 u.mem_write(owner,bytes([kind]));u.mem_write(owner+0x98,bytes([state]));u.mem_write(verts,wire[40:88])
 for i in range(4):
  u.mem_write(edges+i*32,struct.pack('<I',verts+i*12));u.mem_write(edges+i*32+0x14,struct.pack('<II',edges+((i+1)%4)*32,edges+((i-1)%4)*32))
 u.mem_write(query+0x40,wire[192:204]);u.mem_write(query+0x4c,wire[204:208]+struct.pack('<I',0x461));u.mem_write(query+0x54,wire[136:160]);u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
 u.mem_write(stack,struct.pack('<4I',stop,face,query,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4dec10,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit
 if n<6:assert hit==int(n in (0,1)),('analytic',n,hit)
 count=struct.unpack('<I',u.mem_read(out,4))[0];edge=struct.unpack('<I',u.mem_read(out+32,4))[0];edge_hits+=bool(hit and edge);multiple_hits+=count>1
 expected.append(struct.pack('<iI',0,hit)+(bytes(u.mem_read(out+4,32))+struct.pack('<I',count) if hit else bytes([0xa5])*36))
original_count=len(cases)
for offset,value in [(204,-1),(204,math.nan),(160,math.nan),(136,math.inf),(192,math.nan)]:
 wire=bytearray(cases[1]);struct.pack_into('<f',wire,204,.5);struct.pack_into('<f',wire,offset,value)
 struct.pack_into('<IIiIII',wire,164,0x461,0,0,0,0,0)
 cases.append(bytes(wire));expected.append(struct.pack('<iI',-2,0xa5a5a5a5)+bytes([0xa5])*36)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--sweep'],input=b''.join(cases))
assert len(actual)==len(expected)*44,(len(actual),len(expected)*44)
for n,want in enumerate(expected):assert actual[n*44:(n+1)*44]==want,('PC',n,actual[n*44:(n+1)*44].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_sweep_face\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire);face=base+4096;out=base+8192
 x.mem_write(face,wire[:40]+struct.pack('<II',base+40,4)+wire[164:188]);x.mem_write(out,bytes([0xa5])*40)
 limit_bits=struct.unpack_from('<I',wire,160)[0]
 x.mem_write(stack,struct.pack('<9I',stop,face,base+136,base+148,base+192,struct.unpack_from('<I',wire,204)[0],limit_bits,out,out+36));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+36,4))+bytes(x.mem_read(out,36))
 assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',nxdk_cases=len(cases),nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),original_cases=original_count,port_guards=5,hits=hits,edge_hits=edge_hits,multiple_hits=multiple_hits,scope='Complete steady-state 4dec10 geometry with filters and all callees unchanged, fresh hit count. Exact fraction/contact/normal, edge classification and improving-hit count, including separate normal displacement and subthreshold radius, six analytic branch fixtures and rotated polygons. No texture flags or world traversal.')
(root/'artifacts/collision-sweep-verification.json').write_text(json.dumps(report,indent=2));print(report)

