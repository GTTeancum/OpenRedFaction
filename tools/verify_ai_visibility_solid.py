"""Full original4ce740 through uncached flat collision versus PC/NXDK solid binding."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);solid=base;faces=base+4096;verts=base+8192;edges=base+12288;query=base+16384;out=base+20480;stack=base+60000;stop=base+64000
rng=random.Random(0x4ce740);cases=[];expected=[];blocked=0
node=base+25000;point=base+26000
for n in range(2048):
 z=[rng.choice([-4,-2,0,2,4,6]) for _ in range(4)];count=rng.randrange(5);flags=0x45
 start=[rng.choice([0,1,2.25,3]),rng.choice([0,1,2.25]),8];end=[start[0]+rng.choice([-1,0,1]),start[1],-8] if n%17 else start
 radius=rng.choice([0,.00001,.25,.5,1]);wire=struct.pack('<4f2I20f',*z,count,flags,*start,*end,radius,1,*([0]*12));cases.append(wire)
 u.mem_write(solid,bytes(256));u.mem_write(solid+0x70,struct.pack('<I',faces if count else 0))
 for i in range(4):
  f=faces+i*128;v=verts+i*64;e=edges+i*128;bounds=struct.pack('<6f',-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001)
  u.mem_write(f,bytes(128));u.mem_write(f,struct.pack('<4f',0,0,1,-z[i])+bounds);u.mem_write(f+0x30,struct.pack('<i',-1));u.mem_write(f+0x40,struct.pack('<I',e));u.mem_write(f+0x54,struct.pack('<I',f+128 if i+1<count else 0))
  u.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
  for j in range(4):u.mem_write(e+j*32,struct.pack('<I',v+j*12)+bytes(16)+struct.pack('<II',e+((j+1)%4)*32,e+((j-1)%4)*32))
 u.mem_write(node,bytes(68));u.mem_write(node+12,wire[24:36]);u.mem_write(point,wire[36:48]);u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
 u.mem_write(stack,struct.pack('<6I',stop,solid,node,point,struct.unpack_from('<I',wire,48)[0],0x7fc00000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x4ce740,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX)&255;blocked+=result==0;expected.append(struct.pack('<2I',0,result))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--ai-visible-solid'],input=b''.join(cases));assert raw==b''.join(expected),'PC'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
entry=int(re.search(r'_rf_entity_navigation_visible_solid\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 z=struct.unpack_from('<4f',wire);count=struct.unpack_from('<I',wire,16)[0]
 for i in range(4):
  v=verts+i*64;bounds=struct.pack('<6f',-2.0001,-2.0001,z[i]-.0001,2.0001,2.0001,z[i]+.0001)
  x.mem_write(faces+i*72,struct.pack('<4f',0,0,1,-z[i])+bounds+struct.pack('<8I',v,4,0,0,0,0,0,0));x.mem_write(v,struct.pack('<12f',-2,-2,z[i],2,-2,z[i],2,2,z[i],-2,2,z[i]))
 x.mem_write(solid,bytes(156));x.mem_write(solid+148,struct.pack('<2I',faces,count));x.mem_write(node,bytes(68));x.mem_write(node+12,wire[24:36]);x.mem_write(point,wire[36:48]);x.mem_write(out,struct.pack('<I',99))
 x.mem_write(stack,struct.pack('<7I',stop,solid,node,point,struct.unpack_from('<I',wire,48)[0],0x7fc00000,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,4));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_pc_nxdk_cases=2048,blocked=blocked,scope='Full unhooked original4ce740 and4df1c0 flat-world path versus concrete shared solid binding. Real polygons, thin/swept radii, empty lists, zero motion, edge approaches and blocked/clear segments. No room hierarchy, preferred/cache faces or scene integration in this verifier.')
(root/'artifacts/ai-visibility-solid.json').write_text(json.dumps(report,indent=2));print(report)
