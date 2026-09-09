"""Compare uncached local-space thin room queries with complete original calls."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000
solid=base;rooms=base+1024;nodes=base+4096;faces=base+8192;verts=base+12288;edges=base+16384;query=base+20480;out=base+24576;arrays=base+28672
rng=random.Random(0x4df1c0);cases=[];expected=[];hits=multiple=0
for n in range(2400):
 records=[]
 for i in range(4):
  z=rng.choice([-4,-2,0,2,4]);lo=[-5,-5,rng.choice([-10,z])];hi=[5,5,rng.choice([10,z])]
  records.append(struct.pack('<7f3I',*lo,*hi,z,rng.choice([0,0,1,255]),i,rng.randrange(5-i)))
 primary=rng.sample(range(4),2);children=[rng.randrange(4) for _ in range(4)]
 start=[rng.choice([0,2,3,6]),0,8];delta=[rng.choice([-1,0,1]),0,-16] if n%17 else [0,0,0]
 limit=rng.choice([.25,.5,1]);flags=rng.choice([0x464,0x465,0x46c,0x46d])
 wire=b''.join(records)+struct.pack('<6I7fI',*primary,*children,*start,*delta,limit,flags);cases.append(wire)
 u.mem_write(solid,bytes(512));u.mem_write(solid+0x90,struct.pack('<I',1));u.mem_write(solid+0x9c,struct.pack('<III',2,2,arrays));u.mem_write(arrays,struct.pack('<2I',*[rooms+i*512 for i in primary]))
 for i,record in enumerate(records):
  r=rooms+i*512;node=nodes+i*64;face=faces+i*128;v=verts+i*64;e=edges+i*128;z=struct.unpack_from('<f',record,24)[0];skip,first,count=struct.unpack_from('<3I',record,28)
  bounds=struct.pack('<6f',-2.0001,-2.0001,z-.0001,2.0001,2.0001,z+.0001)
  u.mem_write(r,bytes(512));u.mem_write(r+1,bytes([skip]));u.mem_write(r+8,record[:24]);u.mem_write(r+0x3c,struct.pack('<I',node));u.mem_write(r+0x6c,struct.pack('<III',count,count,arrays+64+i*32));u.mem_write(arrays+64+i*32,struct.pack('<'+str(count)+'I',*[rooms+j*512 for j in children[first:first+count]]))
  u.mem_write(node,bounds+struct.pack('<4I',face,1,0,0));u.mem_write(face,bytes(128));u.mem_write(face,struct.pack('<4f',0,0,1,-z)+bounds);u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x40,struct.pack('<I',e))
  u.mem_write(v,struct.pack('<12f',-2,-2,z,2,-2,z,2,2,z,-2,2,z))
  for j in range(4):u.mem_write(e+j*32,struct.pack('<I',v+j*12)+bytes(16)+struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(query,bytes(128));u.mem_write(query+0x34,struct.pack('<6ffI',*start,*delta,0,flags));u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15))
 u.mem_write(stack,struct.pack('<4I',stop,query,out,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,solid);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4df1c0,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 count=struct.unpack('<I',u.mem_read(out,4))[0];hits+=bool(count);multiple+=count>1
 if count:
  index=(struct.unpack('<I',u.mem_read(out+36,4))[0]-faces)//128;assert 0<=index<4
  value=bytes(u.mem_read(out+4,28))+struct.pack('<3I',0,count,index)
 else:value=bytes([0xa5])*40
 expected.append(struct.pack('<iI',0,bool(count))+value)
for offset,value,status in [(160,99,-4),(28,256,-4),(32,99,-4),(0,math.nan,-2),(208,math.nan,-2),(212,0x1000,-3)]:
 wire=bytearray(cases[0]);struct.pack_into('<f' if isinstance(value,float) else '<I',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',status)+bytes([0xa5])*44)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--room-query'],input=b''.join(cases))
assert len(raw)==len(cases)*48
for n,want in enumerate(expected):assert raw[n*48:(n+1)*48]==want,(n,raw[n*48:(n+1)*48].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_thin_rooms\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16);trees=base+32768;work=base+36864
for n,(wire,want) in enumerate(zip(cases,expected)):
 for i in range(4):
  record=wire[i*40:(i+1)*40];z=struct.unpack_from('<f',record,24)[0];node=nodes+i*40;face=faces+i*72;v=verts+i*64
  bounds=struct.pack('<6f',-2.0001,-2.0001,z-.0001,2.0001,2.0001,z+.0001)
  x.mem_write(rooms+i*40,record[:24]+record[28:40]+struct.pack('<I',trees+i*40))
  x.mem_write(trees+i*40,struct.pack('<10I',0,node,face,0,work+i*4,1,1,1,0,0))
  x.mem_write(node,bounds+struct.pack('<4I',0,1,0xffffffff,0xffffffff))
  x.mem_write(face,struct.pack('<4f',0,0,1,-z)+bounds+struct.pack('<8I',v,4,0,0,0,0,0,0))
  x.mem_write(v,struct.pack('<12f',-2,-2,z,2,-2,z,2,2,z,-2,2,z))
 x.mem_write(arrays,wire[160:184]);x.mem_write(query,wire[184:208]);x.mem_write(out,bytes([0xa5])*44)
 limit_bits,flags=struct.unpack_from('<2I',wire,208)
 x.mem_write(stack,struct.pack('<13I',stop,rooms,4,arrays,2,arrays+8,4,flags,query,query+12,limit_bits,out,out+40));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+40,4))+bytes(x.mem_read(out,40))
 assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=2400,port_guards=6,hits=hits,multiple_updates=multiple,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),scope='Complete unmodified 4df1c0 with zero radius, direct local coordinates, no preferred face/cache, hierarchy enabled and special mode 1000 disabled; unchanged tree/face queries. Four single-face room trees, ordered primary/detail lists, skip flags, overlap rejection, ties and first/nearest queries. PC and actual NXDK-linked comparison; not XEMU gameplay.')
(root/'artifacts/collision-room-query-verification.json').write_text(json.dumps(report,indent=2));print(report)
