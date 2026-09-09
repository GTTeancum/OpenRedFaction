"""Complete original segment/AABB function, including failed-attempt writes."""
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

rng=random.Random(0x4deab0);cases=[];expected=[];hits=0;multiple=0
for n in range(1800):
 z=[rng.choice([-4,-2,0,2,4]) for _ in range(3)];flags=rng.choice([0x460,0x461]);start=[rng.choice([0,1,3]),0,8];delta=[rng.choice([0,-1,1]),0,-16];limit=rng.choice([.5,1])
 nodes=[]
 for i in range(3):
  lo=[-20,-20,-20] if not i else [-2.0001,-2.0001,z[i]-.0001];hi=[20,20,20] if not i else [2.0001,2.0001,z[i]+.0001]
  left,right=(1,2) if not i else (0xffffffff,0xffffffff)
  if not i and n%2:left,right=right,left
  nodes.append(struct.pack('<6f4I',*lo,*hi,i,rng.randrange(2),left,right))
 wire=b''.join(nodes)+struct.pack('<10fI',*z,*start,*delta,limit,flags);cases.append(wire)
 original_nodes=base;faces=base+4096;query=base+8192;out=base+12288;verts=base+16384;edges=base+20480
 for i in range(3):
  node=original_nodes+i*64;face=faces+i*128;v=verts+i*64;e=edges+i*128
  first,count,left,right=struct.unpack_from('<4I',nodes[i],24)
  u.mem_write(node,nodes[i][:24]+struct.pack('<4I',face if count else 0,0,original_nodes+left*64 if left!=0xffffffff else 0,original_nodes+right*64 if right!=0xffffffff else 0))
  u.mem_write(face,bytes(128));u.mem_write(face,struct.pack('<10f',0,0,1,-z[i],-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001));u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x40,struct.pack('<I',e))
  u.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
  for j in range(4):
   u.mem_write(e+j*32,struct.pack('<I',v+j*12));u.mem_write(e+j*32+0x14,struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(query+0x4c,struct.pack('<fI6f',0,flags,*start,*delta));u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06b0,struct.pack('<I',15))
 u.mem_write(stack,struct.pack('<4I',stop,original_nodes,query,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4deab0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 count=struct.unpack('<I',u.mem_read(out,4))[0];hit=int(count>0);hits+=hit;multiple+=count>1
 if hit:
  face=struct.unpack('<I',u.mem_read(out+36,4))[0];index=(face-faces)//128;assert 0<=index<3
  result=bytes(u.mem_read(out+4,28))+struct.pack('<II',index,count)
 else:result=bytes([0xa5])*36
 expected.append(struct.pack('<iI',0,hit)+result)
original_count=len(cases)
for guard in range(3):
 wire=bytearray(cases[0]);status=-4
 if guard==0:struct.pack_into('<I',wire,32,99)
 elif guard==1:struct.pack_into('<f',wire,156,math.nan);status=-2
 else:
  struct.pack_into('<4I',wire,24,0,0,0,0xffffffff);status=-2
 cases.append(bytes(wire));expected.append(struct.pack('<iI',status,0xa5a5a5a5)+bytes([0xa5])*36)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--tree'],input=b''.join(cases))
assert len(actual)==len(expected)*44
for n,want in enumerate(expected):assert actual[n*44:(n+1)*44]==want,(n,actual[n*44:(n+1)*44].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_thin_tree\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 x.mem_write(base,wire[:120]);faces=base+4096;query=base+8192;out=base+12288;verts=base+16384
 z=struct.unpack_from('<3f',wire,120);flags=struct.unpack_from('<I',wire,160)[0];limit_bits=struct.unpack_from('<I',wire,156)[0]
 for i in range(3):
  v=verts+i*64
  x.mem_write(faces+i*72,struct.pack('<10f8I',0,0,1,-z[i],-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001,v,4,0,0,0,0,0,0))
  x.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
 x.mem_write(query,wire[132:156]);x.mem_write(out,bytes([0xa5])*40)
 x.mem_write(stack,struct.pack('<13I',stop,base,3,faces,3,flags,query,query+12,limit_bits,base+26000,3,out,out+36));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+36,4))+bytes(x.mem_read(out,36))
 assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_count,port_guards=3,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),cases=len(cases),hits=hits,multiple_updates=multiple,scope='Complete original 4deab0 with unchanged stack probe, bounds expansion at radius zero, linked face iteration and face queries. Three-node trees, child order swaps, empty lists, ties, closest/first-hit modes. No tree construction or world room selection.')
(root/'artifacts/collision-tree-verification.json').write_text(json.dumps(report,indent=2));print(report)
