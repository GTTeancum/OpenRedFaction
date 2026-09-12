"""Compare original4dec10 alpha-tested thin collision with PC/NXDK; UV/bitmap supplied."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

texture_calls=0;sample_point=b'\xa5'*12;extra=(0,0,0,1);uv_calls=0

def texture_hook(cpu,address,size,context):
 global texture_calls,sample_point,uv_calls
 sp=cpu.reg_read(UC_X86_REG_ESP);read=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0];arg=lambda n:read(sp+4+4*n);pop=0;value=0
 if address==0x4e1ad0:
  assert cpu.reg_read(UC_X86_REG_ECX)==base;sample_point=bytes(cpu.mem_read(arg(0),12));uv_calls+=1
  cpu.mem_write(arg(1),struct.pack('<f',.25));cpu.mem_write(arg(2),struct.pack('<f',.75));pop=12
 elif address==0x50e330:
  assert arg(0)==extra[0]&0xffffffff and arg(1)==0x3e800000 and arg(2)==0x3f400000
  cpu.mem_write(arg(3),struct.pack('<I',extra[1]));texture_calls+=1;value=extra[1]&1
 else:
  assert arg(0)==77 and arg(2)==extra[0]&0xffffffff
  sample_point=bytes(cpu.mem_read(arg(3),12));texture_calls+=1
  if extra[2]:value=0xffffffff
  else:cpu.mem_write(arg(4),struct.pack('<I',extra[1]))
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop);cpu.reg_write(UC_X86_REG_EIP,read(sp))
for a in (0x4e1ad0,0x50e330):u.hook_add(UC_HOOK_CODE,texture_hook,begin=a,end=a)
rng=random.Random(0x4dec10);cases=[];expected=[];hits=0
sampled=0;transparent=0;failures=0;missing=0
for n in range(6060):
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
 limit=rng.choice([.25,.5,1]);flags=rng.choice([0,0,0,1,4,0x40,0x80,0x2000]);prop=rng.choice([-1,0,0,1]);present=rng.randrange(2);kind=rng.choice([0,1,2]);state=rng.choice([0,1])
 if n>=6000:
  # Finite coplanar rays: all axes/signs, interior and boundary, including zero length.
  height=0;plane=normal+[0];start=[0,0,0];delta=[0,0,0]
  for v in vertices:v[axis]=0
  lo[axis]=hi[axis]=0
  start[(axis+1)%3]=[0,-4,4][(n//3)%3]
  delta[(axis+2)%3]=[0,1,-8,8][(n//9)%4]
  flags=0;prop=-1;present=0;kind=state=0
 qflags=0x5|rng.choice([0,0x80,0x100,0x180]);flags=rng.choice([0,0x40,0x80,0xc0,1]);prop=-1;present=0
 extra=(rng.choice([-2,-1,0,3]),(rng.choice([0,1,127,128,129,255])<<24)|rng.randrange(0x1000000),int(n>=5800 and n%3==0),int(n<5800 or n%3!=1))
 if 5800<=n<6000:
  start=[0,0,0];delta=[0,0,0];start[axis]=height+2*sign;delta[axis]=-4*sign;limit=1;flags=0xc0;qflags=0x185;extra=(3,extra[1],extra[2],extra[3])
 texture_calls=uv_calls=0;sample_point=b'\xa5'*12
 filter=[qflags,flags,prop,present,kind,state]
 wire=struct.pack('<41fIIiIIII',*plane,*lo,*hi,*[v for vert in vertices for v in vert],*([0]*12),*start,*delta,limit,*filter,4)+struct.pack('<i3I',*extra);cases.append(wire)
 # Round source data to the binary32 wire representation before mapping original views.
 face=base;query=base+4096;owner=base+8192;edges=base+12288;out=base+16384;verts=base+20480
 u.mem_write(face,wire[:16]);u.mem_write(face+0x10,wire[16:40]);u.mem_write(face+0x28,struct.pack('<I',flags));u.mem_write(face+0x30,struct.pack('<i',extra[0]));u.mem_write(face+0x34,struct.pack('<h',prop));u.mem_write(face+0x40,struct.pack('<II',edges,owner if present else 0))
 u.mem_write(owner,bytes([kind]));u.mem_write(owner+0x98,bytes([state]));u.mem_write(verts,wire[40:88])
 for i in range(4):
  u.mem_write(edges+i*32,struct.pack('<I',verts+i*12));u.mem_write(edges+i*32+0x14,struct.pack('<II',edges+((i+1)%4)*32,edges+((i-1)%4)*32))
 u.mem_write(query+0x4c,struct.pack('<fI',0,qflags));u.mem_write(query+0x54,wire[136:160]);u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06b0,struct.pack('<I',15))
 u.mem_write(stack,struct.pack('<4I',stop,face,query,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4dec10,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255;assert hit in (0,1);hits+=hit
 assert texture_calls==uv_calls and texture_calls<=1
 sampled+=texture_calls;transparent+=texture_calls and not hit
 if texture_calls and (extra[2] or not extra[3]):
  status=-1 if extra[3] else -3;failures+=bool(extra[3]);missing+=not extra[3]
  tail=struct.pack('<I',1)+sample_point if extra[3] else struct.pack('<I',0)+b'\xa5'*12
  expected.append(struct.pack('<i',status)+b'\xa5'*32+tail)
 else:expected.append(struct.pack('<iI',0,hit)+(bytes(u.mem_read(out+4,28)) if hit else b'\xa5'*28)+struct.pack('<I',texture_calls)+sample_point)
assert all(struct.unpack_from("<I",v,4)[0]==0 for v in expected[6000:6060]), "original coplanar misses"
original_count=len(cases)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--thin-textured'],input=b''.join(cases))
assert len(actual)==len(expected)*52,(len(actual),len(expected)*52)
for n,want in enumerate(expected):assert actual[n*52:(n+1)*52]==want,('PC',n,actual[n*52:(n+1)*52].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_thin_face_textured\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
backend=base+0x6000;callback=backend+0x100;x.mem_write(backend,struct.pack('<2I',callback,77));x.hook_add(UC_HOOK_CODE,texture_hook,begin=callback,end=callback)
for n,(wire,want) in enumerate(zip(cases,expected)):
 extra=struct.unpack_from('<i3I',wire,192);texture_calls=0;sample_point=b'\xa5'*12
 x.mem_write(base,wire);face=base+4096;out=base+8192
 x.mem_write(face,wire[:40]+struct.pack('<II',base+40,4)+wire[164:188]);x.mem_write(out,bytes([0xa5])*32)
 limit_bits=struct.unpack_from('<I',wire,160)[0]
 x.mem_write(stack,struct.pack('<9I',stop,face,extra[0]&0xffffffff,base+136,base+148,limit_bits,backend if extra[3] else 0,out,out+28));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+28,4))+bytes(x.mem_read(out,28))+struct.pack('<I',texture_calls)+sample_point
 assert got==want,('NXDK',n,got.hex(),want.hex())
assert sampled and transparent and failures and missing,(sampled,transparent,failures,missing)
report=dict(result='PASS',cases=6060,sampled=sampled,transparent=transparent,callback_errors=failures,missing_backend_errors=missing,original_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='Full zero-radius original4dec10, actual filters/geometry/color constructor; only UV4e1ad0 and bitmap50e330 supplied. Exact PC/NXDK match result, fraction/point/normal, sampler call/point. Paired query/face flags, negative bitmap bypass, alpha127/128 boundary, geometry misses and coplanar cases. Port sample failures/missing required backend preserve outputs. Actual UV interpolation/texture decoding, swept edge path and scene binding remain open.')
(root/'artifacts/collision-textured-thin.json').write_text(json.dumps(report,indent=2));print(report)
