"""Original special lightmap grid with supplied retained topology vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_accumulate_special\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=20000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

import math
rng=random.Random(0x4f3500);inputs=[];responses=[];pixels=0
SOURCE=B+0x3000;MASK=B+0x7800;POLYS=B+0x5000;TABLE=B+0x8000;FACES=B+0x8100;NODES=B+0x9000;NORMALS=B+0xa000
for a,v in [(0x5a38c4,0),(0x5a38cc,1),(0x1818b84,0)]:o.mem_write(a,w(v))
branches={'prefix':0,'exhausted':0,'vertex':0,'edges':0}
def observe(u,address,size,user):
 if address==0x4f3da9:branches['prefix']+=1
 elif address==0x4f39a7:branches['exhausted']+=1
 elif address==0x4f3d29:branches['vertex' if u.mem_read(STACK+0x13,1)[0] else 'edges']+=1
o.hook_add(UC_HOOK_CODE,observe)
for i in range(256):
 width=2+i%7;height=2+(i//7)%7;count=i%5;masked=i%2;pc=i%5;counts=[rng.randrange(1,9) for j in range(4)]
 iw,ih=rng.choice([(16,16),(17,19),(32,29)]);ox,oy=2,3
 vertices=[[[rng.uniform(0,.7),rng.uniform(0,.7)]+[rng.uniform(-3,3) for k in range(3)]+[rng.uniform(-1,1) for k in range(3)] for j in range(8)] for face in range(4)]
 if i%8==0:
  pc=1;counts[0]=4
  for j,uv in enumerate([(.15,.2),(.45,.2),(.45,.55),(.15,.55)]):vertices[0][j][:2]=uv
 if i%8==1:
  pc=1;counts[0]=2
  vertices[0][0][:2]=[0,(oy+.5)/ih];vertices[0][1][:2]=[1,(oy+.5)/ih]
 sources=[]
 for j in range(4):sources.append(w(1+(i+j)%4,j%4)+f(*[rng.uniform(-4,4) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],*[rng.uniform(0,.5) for _ in range(3)],rng.choice([1,5,20]),rng.random(),-.5,.75)+w(j%2))
 channels=[f(*[rng.uniform(-.05,.1) for _ in range(64)]) for j in range(3)];masks=[bytes(rng.choice([0,1,32,127,255]) for _ in range(64)) for j in range(4)]
 data=w(iw,ih,ox,oy,width,height,count,masked,pc,*counts)+b''.join(f(*v) for face in vertices for v in face)+b''.join(sources)+b''.join(channels)+b''.join(masks);assert len(data)==2404;inputs.append(data)
 o.mem_write(B,data);o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+9,b'\x01');o.mem_write(OWNER+12,w(IMAGE,ox,oy,width,height));o.mem_write(IMAGE,w(0,iw,ih,OUT));o.mem_write(0x5a38e0,f(.25));o.mem_write(0xc9687c,w(count))
 for j,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,channels[j])
 for j,source in enumerate(sources):
  p=SOURCE+j*256;o.mem_write(p,bytes(256));o.mem_write(p+8,source[:4]);o.mem_write(p+12,source[8:44]);o.mem_write(p+0x38,source[60:64]);o.mem_write(p+0x3c,source[56:60]);o.mem_write(p+0x40,source[44:56]);o.mem_write(p+0x4e,source[72:73]);o.mem_write(p+0x54,source[4:8]);o.mem_write(p+0x84,source[64:72]);o.mem_write(0xc4d588+j*4,w(p));o.mem_write(MASK+j*4,w(B+2148+j*64))
 for j in range(4):
  face=FACES+j*80;node=NODES+j*256;norm=NORMALS+256+j*128
  o.mem_write(TABLE+j*4,w(face));o.mem_write(face+64,w(node));o.mem_write(NORMALS+j*12,w(counts[j],counts[j],norm))
  for k in range(counts[j]):
   p=B+52+j*256+k*32;next_node=node+(k+1)*24 if k+1<counts[j] else (node if i%2 else 0)
   o.mem_write(node+k*24,w(p+8)+bytes(8)+bytes(o.mem_read(p,8))+w(next_node));o.mem_write(norm+k*12,bytes(o.mem_read(p+20,12)))
 invw=struct.unpack('<f',f(1/iw))[0];invh=struct.unpack('<f',f(1/ih))[0];radius=f(math.sqrt((1/ih)*invh+invw*invw)*.5)
 o.mem_write(STACK,bytes(1024))
 for offset,value in [(0x38,0x1431de0),(0x28,0x14b23e0),(0x1c,0x13f1de0),(0x48,NORMALS),(0x298,OWNER),(0x2a0,MASK if masked else 0)]:o.mem_write(STACK+offset,w(value))
 o.mem_write(STACK+0x54,f(invw,invh));o.mem_write(STACK+0x2c,radius);o.mem_write(STACK+0x5c,w(pc,pc,TABLE))
 o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBX,0);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f)
 o.emu_start(0x4f3500,0x4f3e23,count=4000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f3e23
 # Topology is supplied, so skip only its destruction and execute original border copies.
 o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4f3e5d,0x4f3f4c,count=100000)
 expected=b''.join(bytes(o.mem_read(a,256)) for a in [0x1431de0,0x14b23e0,0x13f1de0]);pixels+=width*height
 x.mem_write(B,data);x.mem_write(MASK,w(*(B+2148+j*64 for j in range(4))))
 x.mem_write(VIEW,w(iw,ih,ox,oy)+bytes(40)+w(width,height,B+1076,count,MASK if masked else 0,64)+f(.25)+w(B+1380,B+1636,B+1892,64))
 for j in range(4):x.mem_write(POLYS+j*8,w(B+52+j*256,counts[j]))
 assert call([VIEW,POLYS,pc])==0
 got=bytes(x.mem_read(B+1380,768));assert got==expected,(i,next((j for j in range(192) if got[j*4:j*4+4]!=expected[j*4:j*4+4]),-1));responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-special-grid'],input=b''.join(inputs))==b''.join(responses)
for offset,value in [(56,1),(68,65),(96,0),(76,0)]:
 before=bytes(x.mem_read(B+1380,768));saved=bytes(x.mem_read(VIEW+offset,4));x.mem_write(VIEW+offset,w(value))
 assert call([VIEW,POLYS,pc])!=0 and bytes(x.mem_read(B+1380,768))==before;x.mem_write(VIEW+offset,saved)
assert all(branches.values()),branches
report=dict(branches=branches,nxdk_binding_guards=4,result='PASS',original_pc_nxdk_grids=len(inputs),pixels=pixels,original_sha256=sha,x87_control_word='0x027f',scope='Original4f3500..4f3e23 full special texel loop including actual selection/interpolation/4da8b0, followed by unhooked border copies4f3e5d..4f3f4a. Supplied topology, normals, radius, selected sources/masks/initial planes; allocation/collection/destruction and native resource binding excluded.')
(root/'artifacts/lightmap-special-grid.json').write_text(json.dumps(report,indent=2));print(report)

