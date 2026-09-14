"""Replay full original shadow receiver filtering and mask filling against PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_geometry_shadow_traverse\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
seen=[]
def observe(u,a,size,ctx):seen.append(a)
for address in (0x4f5031,0x4f53bd,0x4f2100):o.hook_add(UC_HOOK_CODE,observe,begin=address,end=address)
rng=random.Random(0x4f4f15);inputs=[];responses=[];totals=[0]*4;changed=0
for case in range(256):
 planes=[[0,0,-1,0],[0,0,1,-10],[1,0,0,-4],[-1,0,0,-4],[0,1,0,-4],[0,-1,0,-4]]
 cull=f(*([-8]*3),*([8]*3),-5,-5,0,5,5,10,0,0,1,-10,*[v for p in planes for v in p])+w(0)
 sample=w(128,128,8,8)+f(.01,.01,.1,.1,0,0,1,-10)+w(2,0)
 prepared=sample+w(16,16)+f(0,0,0)+b''.join(f(*p) for p in planes)
 raw=bytearray(1472);formats=[];receiver=f(-20,-20,40,-20,40,40,-20,40);threshold=f(rng.choice([0,.1,4,100]),1);initial=rng.randbytes(1024)
 o.mem_write(STACK,bytes(4096));o.mem_write(OUT,initial);o.mem_write(STACK+0x40,w(1,1,B+0x1800));o.mem_write(B+0x1000,receiver+bytes(224)+w(4));o.mem_write(B+0x1800,w(B+0x1000));o.mem_write(STACK+0x70,w(127));o.mem_write(STACK+0x64,f(0,0,0));o.mem_write(STACK+0x4c,cull[:24]);o.mem_write(STACK+0xd4,cull[24:36]);o.mem_write(STACK+0xf8,cull[36:48]);o.mem_write(STACK+0x1a0,cull[64:160]);o.mem_write(B+0xf000,bytes(32));o.mem_write(B+0xf00c,w(OWNER));o.mem_write(B+0xf018,w(OUT))
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,8,8,16,16));o.mem_write(OWNER+256,w(0,128,128));o.mem_write(OWNER+0x2c,threshold);o.mem_write(OWNER+0x4c,f(.01,.01,.1,.1)+w(2,0,1));o.mem_write(OWNER+0x6c,f(0,0,1,-10));o.mem_write(0x5a4554,w(32));o.mem_write(0x17c80c4,w(B+0x9000));o.mem_write(B+0x9000,bytes(108*8))
 for j,p in enumerate(planes):
  bits=0
  for v in p[:3]:bits=bits*2+(v>0)
  o.mem_write(STACK+0x260+j*4,w([4,0,5,1,7,3,6,2][bits]))
 seen.clear()
 for face in range(8):
  cx=rng.uniform(-6,6);cy=rng.uniform(-6,6);z=rng.uniform(1,11);r=rng.uniform(.1,3)
  verts=[(cx-r,cy-r,z),(cx+r,cy-r,z),(cx+r,cy+r,z),(cx-r,cy+r,z)];packed=b''.join(f(*p) for p in verts);verts=[struct.unpack_from('<3f',packed,j*12) for j in range(4)];z=verts[0][2]
  raw[face*48:face*48+48]=packed;offset=384+face*136;flags=rng.choice([0,0,0,4,64,8192]);portal=rng.choice([0,0,-1,1]);mapping=rng.choice([0,1,1,1]);fmt=rng.choice([0,1,4,5,6,7,0xffffffff]);formats.append(fmt)
  header=bytearray(56);header[:16]=f(0,0,1,-z);header[16:24]=w(face,mapping);header[36:44]=w(portal&0xffffffff,flags);header[48:56]=w(0,4);raw[offset:offset+56]=header
  for j in range(4):raw[offset+56+j*20:offset+76+j*20]=w(face*4+j)+bytes(16)
  minimum=[struct.unpack('<f',f(min(v[j] for v in verts)-struct.unpack('<f',f(.0001))[0]))[0] for j in range(3)];maximum=[struct.unpack('<f',f(max(v[j] for v in verts)+struct.unpack('<f',f(.0001))[0]))[0] for j in range(3)]
  F=B+0x6000+face*80;V=B+0x3000+face*48;C=B+0x2000+face*96
  o.mem_write(V,packed);o.mem_write(F,bytes(80));o.mem_write(F,f(0,0,1,-z,*minimum,*maximum)+w(flags,0,face if fmt!=0xffffffff else 0xffffffff)+struct.pack('<hh',portal,mapping));o.mem_write(F+64,w(C))
  for j in range(4):o.mem_write(C+j*24,w(V+j*12,0,0,0,0,C+(j+1)%4*24))
  o.mem_write(B+0x9000+face*108+60,w(fmt if fmt!=0xffffffff else 0));o.mem_write(STACK+0x18,w(F));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_EAX,F);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4f15,0x4f5515,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f5515
 expected=bytes(o.mem_read(OUT,1024));counts=[8,seen.count(0x4f5031),seen.count(0x4f53bd),seen.count(0x4f2100)]
 data=cull+prepared+bytes(raw)+w(*formats)+receiver+threshold+initial;assert len(data)==2904;x.mem_write(B,data)
 for i,fmt in enumerate(formats):x.mem_write(B+0x5000+i*20,w(0,0,0,fmt,0));x.mem_write(B+0x5100+i*4,w(B+0x5000+i*20 if fmt!=0xffffffff else 0))
 x.mem_write(B+0x5200,prepared+w(B+0x5600));x.mem_write(B+0x5400,cull);x.mem_write(B+0x5600,w(B+0x5700,1)+threshold+w(B+0x5800,B+0x9c00,64));x.mem_write(B+0x5700,w(B+1840,4));x.mem_write(B+0x5800,w(B+0x9000,B+0x9400,B+0x9800,64));x.mem_write(B+0x8800,bytes(512));x.mem_write(B+0x9c00,bytes(512));x.mem_write(B+0x6200,w(*[384+i*136 for i in range(8)]));x.mem_write(B+0x6300,w(*range(8)));x.mem_write(B+0x6400,w(B+0x8000,B+0x8400,B+0x8800,64));x.mem_write(B+0x6500,w(B+0x6000,4,B+0x6400));x.mem_write(B+0x6100,w(B+336,1472,0,8,1,32,8,32,2,0,0,0,0,0,B+0x6200,0,0));x.mem_write(B+0x6600,bytes([165])*16);x.mem_write(B+0x4000,initial)
 x.mem_write(STACK,w(STOP,B+0x6100,B+0x6300,8,B+0x5100,8,B+0x5400,B+0x5200,B+0x6500,B+0x4000,1024,127,B+0x6600));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);status=x.reg_read(UC_X86_REG_EAX)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and status==0 and bytes(x.mem_read(B+0x6600,16))==w(*counts) and bytes(x.mem_read(B+0x4000,1024))==expected,(case,status,counts,bytes(x.mem_read(B+0x6600,16)).hex())
 inputs.append(data);responses.append(w(0,*counts)+expected);totals=[a+b for a,b in zip(totals,counts)];changed+=sum(a!=b for a,b in zip(initial,expected))
assert subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),'--shadow-traversal'],input=b''.join(inputs))==b''.join(responses)
for guard in range(3):
 x.mem_write(B+0x4000,initial);x.mem_write(B+0x6600,bytes([165])*16);x.mem_write(B+0x6300,w(*range(8)));x.mem_write(B+0x6504,w(4))
 if guard==0:x.mem_write(B+0x6300,w(0,0))
 if guard==1:x.mem_write(B+0x6504,w(3))
 x.mem_write(STACK,w(STOP,B+0x6100,B+0x6300,8,B+0x5100,0 if guard==2 else 8,B+0x5400,B+0x5200,B+0x6500,B+0x4000,1024,127,B+0x6600));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(B+0x6600,16))==bytes([165])*16 and bytes(x.mem_read(B+0x4000,1024))==initial,guard
report=dict(result='PASS',original_pc_nxdk_traversals=len(inputs),nxdk_preflight_guards=3,visited=totals[0],eligible=totals[1],projected=totals[2],accepted=totals[3],changed_mask_bytes=changed,original_sha256=sha,scope='Eight retained serialized faces per traversal, original linked corners and actual texture classification through4f4f15..4f5515 per face. Mask and projection/filter scratch retained across faces; original chunk reentered in supplied file order. Synthetic geometry, explicit prepared volumes/image formats; source ownership and native scene still external.')
(root/'artifacts/geometry-shadow-traverse.json').write_text(json.dumps(report,indent=2));print(report)
