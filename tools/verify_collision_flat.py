"""Complete original no-room collision path compared with PC and NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);solid=base;faces=base+4096;verts=base+8192;edges=base+12288;query=base+16384;out=base+20480;stack=base+60000;stop=base+64000
rng=random.Random(0x4df481);cases=[];expected=[];hits=multiple=edge_hits=0
for n in range(5000):
 z=[rng.choice([-4,-2,0,2,4,6]) for _ in range(4)];count=rng.randrange(5);flags=rng.choice([0x460,0x461,0x464,0x465,0x1464,0x1465])
 start=[rng.choice([0,1,2.25,3]),rng.choice([0,1,2.25]),8];delta=[rng.choice([-1,0,1]),0,-16] if n%17 else [0,0,0];radius=rng.choice([0,.00001,.25,.5,1]);limit=rng.choice([.25,.5,1]);origin=[rng.uniform(-1,1) for _ in range(3)];angle=rng.uniform(-.5,.5);c,s=math.cos(angle),math.sin(angle);matrix=[c,0,s,0,1,0,-s,0,c]
 if n<2:z=[0,2,4,6] if n==0 else [0]*4;count=4;flags=0x465;start=[0,0,8];delta=[0,0,-16];radius=0;limit=1
 wire=struct.pack('<4f2I20f',*z,count,flags,*start,*delta,radius,limit,*origin,*matrix);cases.append(wire)
 u.mem_write(solid,bytes(256));u.mem_write(solid+0x70,struct.pack('<I',faces if count else 0))
 for i in range(4):
  f=faces+i*128;v=verts+i*64;e=edges+i*128;bounds=struct.pack('<6f',-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001)
  u.mem_write(f,bytes(128));u.mem_write(f,struct.pack('<4f',0,0,1,-z[i])+bounds);u.mem_write(f+0x30,struct.pack('<i',-1));u.mem_write(f+0x40,struct.pack('<I',e));u.mem_write(f+0x54,struct.pack('<I',f+128 if i+1<count else 0))
  u.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
  for j in range(4):u.mem_write(e+j*32,struct.pack('<I',v+j*12)+bytes(16)+struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(query,bytes(128));u.mem_write(query+4,wire[56:104]);u.mem_write(query+0x34,wire[24:52]+struct.pack('<I',flags));u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
 u.mem_write(stack,struct.pack('<4I',stop,query,out,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,solid);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4df1c0,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 updates=struct.unpack('<I',u.mem_read(out,4))[0];hit=int(updates>0);hits+=hit;multiple+=updates>1
 if hit:
  edge,index=struct.unpack('<2I',u.mem_read(out+32,8));index=(index-faces)//128;edge_hits+=bool(edge)
  value=bytes(u.mem_read(out+4,28))+struct.pack('<3I',index,updates,edge)
  if n<2:assert index==3 and updates==4 and struct.unpack('<f',u.mem_read(out+4,4))[0]==(.125 if n==0 else .5)
 else:value=bytes([0xa5])*40
 expected.append(struct.pack('<iI',0,hit)+value)
for offset,value in [(48,-1),(48,math.nan),(52,math.nan),(24,math.inf),(56,math.nan),(68,math.nan)]:
 wire=bytearray(cases[1]);struct.pack_into('<I',wire,20,0x460);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*44)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--flat-query'],input=b''.join(cases));assert len(raw)==len(cases)*48
for n,want in enumerate(expected):assert raw[n*48:(n+1)*48]==want,('PC',n,raw[n*48:(n+1)*48].hex(),want.hex())
xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_flat_faces\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match;entry=int(match[1],16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 z=struct.unpack_from('<4f',wire);count,flags=struct.unpack_from('<2I',wire,16)
 for i in range(4):
  v=verts+i*64;bounds=struct.pack('<6f',-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001)
  x.mem_write(faces+i*72,struct.pack('<4f',0,0,1,-z[i])+bounds+struct.pack('<8I',v,4,0,0,0,0,0,0));x.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
 x.mem_write(query,wire);x.mem_write(out,bytes([0xa5])*44)
 x.mem_write(stack,struct.pack('<12I',stop,faces,count,flags,query+24,query+36,query+56,query+68,struct.unpack_from('<I',wire,48)[0],struct.unpack_from('<I',wire,52)[0],out,out+40));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+40,4))+bytes(x.mem_read(out,40));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=5000,port_guards=6,hits=hits,multiple_updates=multiple,edge_hits=edge_hits,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original 4df1c0 uncached zero-room fallback, all callees unchanged. Ordered linked faces, input transforms, thin/swept radii, bit-0 non-early-exit, ties and empty lists. PC and NXDK exact geometric/index/count comparison; no original preferred face, cache or loaded mover binding.')
(root/'artifacts/collision-flat-verification.json').write_text(json.dumps(report,indent=2));print(report)
