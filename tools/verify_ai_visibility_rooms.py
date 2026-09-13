"""Verify full navigation visibility through original and shared room geometry."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000
solid=base;rooms=base+1024;nodes=base+4096;faces=base+8192;verts=base+12288;edges=base+16384;query=base+20480;out=base+24576;arrays=base+28672
rng=random.Random(0x4ce741);cases=[];expected=[];blocked=0
candidate=base+40000;point=base+40100
for n in range(2048):
 records=[]
 for i in range(4):
  z=rng.choice([-4,-2,0,2,4]);lo=[-5,-5,rng.choice([-10,z])];hi=[5,5,rng.choice([10,z])]
  records.append(struct.pack('<7f3I',*lo,*hi,z,rng.choice([0,0,1,255]),i,rng.randrange(5-i)))
 primary=rng.sample(range(4),2);children=[rng.randrange(4) for _ in range(4)]
 start=[rng.choice([0,2,2.25,3,6]),0,8];end=[start[0]+rng.choice([-1,0,1]),0,-8] if n%17 else start
 radius=rng.choice([0,.00001,.0001,.25,.5,1,2])
 wire=b''.join(records)+struct.pack('<6I7fI13f',*primary,*children,*start,*end,1,0x45,radius,*([0]*12));cases.append(wire)
 u.mem_write(solid,bytes(512));u.mem_write(solid+0x90,struct.pack('<I',1));u.mem_write(solid+0x9c,struct.pack('<III',2,2,arrays));u.mem_write(arrays,struct.pack('<2I',*[rooms+i*512 for i in primary]))
 for i,record in enumerate(records):
  r=rooms+i*512;node=nodes+i*64;face=faces+i*128;v=verts+i*64;e=edges+i*128;z=struct.unpack_from('<f',record,24)[0];skip,first,count=struct.unpack_from('<3I',record,28)
  bounds=struct.pack('<6f',-2.0001,-2.0001,z-.0001,2.0001,2.0001,z+.0001)
  u.mem_write(r,bytes(512));u.mem_write(r+1,bytes([skip]));u.mem_write(r+8,record[:24]);u.mem_write(r+0x3c,struct.pack('<I',node));u.mem_write(r+0x6c,struct.pack('<III',count,count,arrays+64+i*32));u.mem_write(arrays+64+i*32,struct.pack('<'+str(count)+'I',*[rooms+j*512 for j in children[first:first+count]]))
  u.mem_write(node,bounds+struct.pack('<4I',face,1,0,0));u.mem_write(face,bytes(128));u.mem_write(face,struct.pack('<4f',0,0,1,-z)+bounds);u.mem_write(face+0x30,struct.pack('<i',-1));u.mem_write(face+0x40,struct.pack('<I',e))
  u.mem_write(v,struct.pack('<12f',-2,-2,z,2,-2,z,2,2,z,-2,2,z))
  for j in range(4):u.mem_write(e+j*32,struct.pack('<I',v+j*12)+bytes(16)+struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(candidate,bytes(68));u.mem_write(candidate+12,wire[184:196]);u.mem_write(point,wire[196:208]);u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
 u.mem_write(stack,struct.pack('<6I',stop,solid,candidate,point,struct.unpack_from('<I',wire,216)[0],0x7fc00000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x4ce740,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX)&255;blocked+=result==0;expected.append(struct.pack('<2I',0,result))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--ai-visible-rooms'],input=b''.join(cases));assert raw==b''.join(expected),'PC'
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_entity_navigation_visible_solid\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
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
 x.mem_write(solid,bytes(156));x.mem_write(solid,struct.pack('<6I',rooms,4,arrays,2,arrays+8,4));x.mem_write(arrays,wire[160:184]);x.mem_write(candidate,bytes(68));x.mem_write(candidate+12,wire[184:196]);x.mem_write(point,wire[196:208]);x.mem_write(out,struct.pack('<I',99))
 x.mem_write(stack,struct.pack('<7I',stop,solid,candidate,point,struct.unpack_from('<I',wire,216)[0],0x7fc00000,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,4));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_pc_nxdk_cases=2048,blocked=blocked,scope='Full unhooked original4ce740 through4df1c0 room hierarchy versus concrete shared visibility. Four real single-face trees, ordered primary/child lists, skip bytes, room overlap, zero motion, thin/swept radii and edge approaches. Cache/preferred faces and live scene routing excluded.')
(root/'artifacts/ai-visibility-rooms.json').write_text(json.dumps(report,indent=2));print(report)
