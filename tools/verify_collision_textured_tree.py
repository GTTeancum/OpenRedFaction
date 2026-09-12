"""Complete original swept collision-tree traversal, including ordered contacts."""
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

trace=[];extra=(0,0,0,0,0);pending=None
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def texture_hook(cpu,address,size,context):
 global pending
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(cpu,sp+4+n*4);pop=0;value=0
 if address==0x4e1ad0:
  index=(cpu.reg_read(UC_X86_REG_ECX)-(base+4096))//128;assert 0<=index<3
  pending=struct.pack('<I',index)+bytes(cpu.mem_read(arg(0),12))
  cpu.mem_write(arg(1),struct.pack('<f',.25));cpu.mem_write(arg(2),struct.pack('<f',.75));pop=12
 elif address==0x50e330:
  index=arg(0);assert pending is not None and read_bytes(pending[:4])==index
  trace.append(pending);cpu.mem_write(arg(3),struct.pack('<I',extra[index]));pending=None
 else:
  index=arg(1);assert arg(0)==77 and index<3 and arg(3)==index
  trace.append(struct.pack('<I',index)+bytes(cpu.mem_read(arg(4),12)))
  if extra[3]==index+1:value=0xffffffff
  else:cpu.mem_write(arg(5),struct.pack('<I',extra[index]))
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
def read_bytes(data):return struct.unpack('<I',data)[0]
def tail(records):
 h=2166136261
 for byte in b''.join(records):h=((h^byte)*16777619)&0xffffffff
 return struct.pack('<II',len(records),h)
for address in (0x4e1ad0,0x50e330):u.hook_add(UC_HOOK_CODE,texture_hook,begin=address,end=address)
rng=random.Random(0x4deab0);cases=[];expected=[];hits=0;multiple=0;edge_hits=0;samples=callback_errors=missing_errors=late_errors=0
for n in range(5000):
 z=[rng.choice([-4,-2,0,2,4]) for _ in range(3)];flags=rng.choice([4,5])|rng.choice([0,0x80,0x100,0x180]);start=[rng.choice([0,1,2.25,3]),0,8];delta=[rng.choice([0,-1,1]),0,-16];limit=rng.choice([.5,1])
 radius=rng.choice([0,.00001,.0001,.25,.5,1,2]);normal_delta=[v+rng.uniform(-.5,.5) for v in delta] if n%3==0 else delta[:]
 nodes=[]
 for i in range(3):
  lo=[-20,-20,-20] if not i else [-2.0001,-2.0001,z[i]-.0001];hi=[20,20,20] if not i else [2.0001,2.0001,z[i]+.0001]
  left,right=(1,2) if not i else (0xffffffff,0xffffffff)
  if not i and n%2:left,right=right,left
  nodes.append(struct.pack('<6f4I',*lo,*hi,i,rng.randrange(2),left,right))
 wire=b''.join(nodes)+struct.pack('<10fI',*z,*start,*delta,limit,flags);wire+=struct.pack('<4f',*normal_delta,radius)
 extra=tuple((rng.choice([0,127,128,255])<<24)|rng.randrange(0x1000000) for _ in range(3))+(rng.randrange(4) if n%7==0 else 0,int(n%17==0))
 wire+=struct.pack('<5I',*extra);cases.append(wire);trace=[];pending=None
 original_nodes=base;faces=base+4096;query=base+8192;out=base+12288;verts=base+16384;edges=base+20480
 for i in range(3):
  node=original_nodes+i*64;face=faces+i*128;v=verts+i*64;e=edges+i*128
  first,count,left,right=struct.unpack_from('<4I',nodes[i],24)
  u.mem_write(node,nodes[i][:24]+struct.pack('<4I',face if count else 0,0,original_nodes+left*64 if left!=0xffffffff else 0,original_nodes+right*64 if right!=0xffffffff else 0))
  u.mem_write(face,bytes(128));u.mem_write(face,struct.pack('<10f',0,0,1,-z[i],-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001));u.mem_write(face+0x28,struct.pack('<I',0xc0));u.mem_write(face+0x30,struct.pack('<i',i));u.mem_write(face+0x40,struct.pack('<I',e))
  u.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
  for j in range(4):
   u.mem_write(e+j*32,struct.pack('<I',v+j*12));u.mem_write(e+j*32+0x14,struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(query+0x40,struct.pack('<3f',*normal_delta));u.mem_write(query+0x4c,struct.pack('<fI6f',radius,flags,*start,*delta));u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
 u.mem_write(stack,struct.pack('<4I',stop,original_nodes,query,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4deab0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 count=struct.unpack('<I',u.mem_read(out,4))[0];hit=int(count>0);hits+=hit;multiple+=count>1
 if hit:
  face=struct.unpack('<I',u.mem_read(out+36,4))[0];index=(face-faces)//128;assert 0<=index<3
  edge=struct.unpack('<I',u.mem_read(out+32,4))[0];edge_hits+=bool(edge)
  result=bytes(u.mem_read(out+4,28))+struct.pack('<III',index,count,edge)
 else:result=bytes([0xa5])*40
 status=0;kept=trace
 if trace and extra[4]:status=-3;kept=[];missing_errors+=1
 else:
  for j,record in enumerate(trace):
   if extra[3]==read_bytes(record[:4])+1:status=-1;kept=trace[:j+1];callback_errors+=1;late_errors+=j>0;break
 samples+=len(trace)
 expected.append((struct.pack('<iI',0,hit)+result if not status else struct.pack('<i',status)+b'\xa5'*44)+tail(kept))
original_count=len(cases)
for guard in range(5):
 wire=bytearray(cases[0]);status=-4
 if guard==0:struct.pack_into('<I',wire,32,99)
 elif guard==1:struct.pack_into('<f',wire,156,math.nan);status=-2
 elif guard==2:
  struct.pack_into('<4I',wire,24,0,0,0,0xffffffff);status=-2
 else:
  struct.pack_into('<f',wire,176,-1 if guard==3 else math.nan);status=-2
 cases.append(bytes(wire));expected.append(struct.pack('<iI',status,0xa5a5a5a5)+bytes([0xa5])*40+tail([]))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--sweep-tree-textured'],input=b''.join(cases))
assert len(actual)==len(expected)*56
for n,want in enumerate(expected):assert actual[n*56:(n+1)*56]==want,(n,actual[n*56:(n+1)*56].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_sweep_tree_textured\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
backend=base+28000;callback=backend+256;bitmaps=backend+512
x.mem_write(bitmaps,struct.pack('<3I',0,1,2));x.hook_add(UC_HOOK_CODE,texture_hook,begin=callback,end=callback)
for n,(wire,want) in enumerate(zip(cases,expected)):
 extra=struct.unpack_from('<5I',wire,180);trace=[];x.mem_write(backend,struct.pack('<3I',bitmaps,0 if extra[4] else callback,77))
 x.mem_write(base,wire[:120]);faces=base+4096;query=base+8192;out=base+12288;verts=base+16384
 z=struct.unpack_from('<3f',wire,120);flags=struct.unpack_from('<I',wire,160)[0];limit_bits=struct.unpack_from('<I',wire,156)[0]
 for i in range(3):
  v=verts+i*64
  x.mem_write(faces+i*72,struct.pack('<10f8I',0,0,1,-z[i],-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001,v,4,0,0xc0,0,0,0,0))
  x.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
 x.mem_write(query,wire[132:156]+wire[164:180]);x.mem_write(out,bytes([0xa5])*44)
 x.mem_write(stack,struct.pack('<16I',stop,base,3,faces,3,flags,query,query+12,query+24,struct.unpack_from('<I',wire,176)[0],limit_bits,base+26000,3,backend,out,out+40));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+40,4))+bytes(x.mem_read(out,40))+tail(trace)
 assert got==want,('NXDK',n,got.hex(),want.hex())
assert samples>0 and callback_errors>0 and missing_errors>0 and late_errors>0
report=dict(result='PASS',samples=samples,callback_errors=callback_errors,missing_backend_errors=missing_errors,late_errors=late_errors,original_cases=original_count,port_guards=5,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),cases=len(cases),hits=hits,multiple_updates=multiple,edge_hits=edge_hits,scope='Complete original 4deab0 with UV/bitmap callbacks supplied, actual stack probe, radius-expanded bounds, linked face iteration and face queries. Three-node trees, child order swaps, empty lists, ties, closest/first-hit modes. Separate normal displacement, subthreshold radii and edge contacts. Indexed sampler order/contact hashes match; errors after prior samples preserve outputs. No world room selection or actor response.')
(root/'artifacts/collision-textured-tree.json').write_text(json.dumps(report,indent=2));print(report)
