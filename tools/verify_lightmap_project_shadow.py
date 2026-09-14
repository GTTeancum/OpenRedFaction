"""Replay original4f5219..4f5320 shadow coordinate projection against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_project_shadow\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f5219);inputs=[];responses=[];low=high=inside=0
# Original static bank is 64 masks with 4096-byte spacing, not dynamic padding.
p=pefile.PE(str(exe));bank=struct.unpack('<64I',p.get_data(0x5a3d3c-p.OPTIONAL_HEADER.ImageBase,256));assert all(b-a==4096 for a,b in zip(bank,bank[1:]))
for i in range(4096):
 normal=i%3;uaxis=[j for j in range(3) if j!=normal][(i//3)%2];vaxis=3-normal-uaxis
 width=2+i%63;height=2+(i//63)%63;iw=128+i%897;ih=128+(i//7)%897;ox=rng.randrange(iw-width+1);oy=rng.randrange(ih-height+1)
 scale=[rng.choice([-1,1])*rng.uniform(.001,2) for _ in range(2)];offset=[rng.uniform(-1,1) for _ in range(2)];point=[rng.uniform(-3,3) for _ in range(3)]
 if i%3==0:
  point[uaxis]=((ox+rng.uniform(1,width-1))/iw-offset[0])/scale[0];point[vaxis]=((oy+rng.uniform(1,height-1))/ih-offset[1])/scale[1]
 view=w(iw,ih,ox,oy)+f(*scale,*offset,0,0,0,0)+w(normal,uaxis);data=view+w(width,height)+f(*point)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,ox,oy,width,height));o.mem_write(OWNER+256,w(0,iw,ih));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis,vaxis))
 o.mem_write(STACK+0x400+12,w(OWNER));o.mem_write(STACK+0xbc,f(*point));o.reg_write(UC_X86_REG_EBP,STACK+0x400);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f)
 o.emu_start(0x4f5219,0x4f5320,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4f5320;expected=bytes(o.mem_read(STACK+0x30,8))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*8);assert call(x,entry,[B,width,height,B+64,OUT])==0
 got=bytes(x.mem_read(OUT,8));assert got==expected,(i,got.hex(),expected.hex());inputs.append(data);responses.append(w(0)+expected)
 for q,extent in zip(struct.unpack('<2f',expected),(width,height)):
  assert 1<=q<=extent-1
  if q==1:low+=1
  elif q==extent-1:high+=1
  else:inside+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-project-shadow'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,w(0)),(4,w(0xffffffff)),(16,f(float('inf'))),(24,f(float('nan'))),(48,w(3)),(52,w(normal)),(56,w(1)),(64+4*uaxis,f(float('nan')))]:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*8);ww,hh=struct.unpack('<II',bad[56:64])
 status=call(x,entry,[B,ww,hh,B+64,OUT]);assert status!=0 and bytes(x.mem_read(OUT,8))==bytes([165])*8
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-project-shadow'],input=bytes(bad))==w(status)+bytes([165])*8
report=dict(result='PASS',original_pc_nxdk_points=len(inputs),low_clamps=low,high_clamps=high,interior_coordinates=inside,pc_nxdk_guards=8,original_mask_slots=64,original_mask_stride=4096,original_sha256=sha,x87_control_word='0x027f',scope='Original4f5219..4f5320 with actual axis/vector helpers, six axis orders, signed scales, origins and 2..64 extents. Original U intermediate float store differs from V. Plane intersection supplied. No native XEMU rendering or complete shadow generation verified.')
(root/'artifacts/lightmap-project-shadow.json').write_text(json.dumps(report,indent=2));print(report)
