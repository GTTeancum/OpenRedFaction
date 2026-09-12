"""Original55cfa0 bitmap sample with supplied lock/release versus PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_geometry_sample_texture\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
B=0x30000000;DATA=B+0x1000;OUT=B+0x2000;S=B+0xe000;STOP=B+0xf000
lock=0;trace=[];width=height=pitch=fmt=0
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 global lock
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+i*4)
 if address==0x55ce00:
  assert arg(0)==17 and arg(1)==0;lock=arg(2);cpu.mem_write(lock,w(0,0,fmt,DATA,width,height,pitch,0));cpu.reg_write(UC_X86_REG_EAX,1);trace.append('lock')
 else:assert arg(0)==lock;trace.append('release')
 cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x55ce00,0x50e310):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(cpu,fn,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cw=0x27f if cpu is x else 0x37f;cpu.reg_write(UC_X86_REG_FPCW,cw);cpu.emu_start(fn,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;assert cpu.reg_read(UC_X86_REG_FPCW)==cw;return cpu.reg_read(UC_X86_REG_EAX)
DATA=B+0x9000;OUT=B+0xa000;VERT=B+0x3000;EDGE=B+0x4000;POINT=B+0x7000;WORK=B+0x6000;IMAGE=B+0x8000
width=height=4;pitch=16;fmt=7;rgba=bytes((i*37+11)&255 for i in range(64));bgra=b''.join(bytes((rgba[i+2],rgba[i+1],rgba[i],rgba[i+3])) for i in range(0,64,4))
u.mem_write(DATA,bgra+bytes(16));swizzled=bytearray(64)
for yy in range(4):
 for xx in range(4):
  index=(xx&1)|((yy&1)<<1)|((xx&2)<<1)|((yy&2)<<2);swizzled[index*4:index*4+4]=rgba[(yy*4+xx)*4:(yy*4+xx+1)*4]
x.mem_write(DATA,bytes(swizzled));x.mem_write(IMAGE,w(4,4,64,7,DATA));results=[];total=uv_misses=bounds=guards=0
levels=json.loads((root/'artifacts/geometry.json').read_text())
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-texture-uv',str(root/'Installed_Game'/level['archive']),level['file']]);at=0;counted=0
 while at<len(raw):
  index,count=struct.unpack_from('<2I',raw,at);assert index==counted and 3<=count<=256;plane=raw[at+8:at+24];point=raw[at+24:at+36];vertices=raw[at+36:at+36+count*12];coords=raw[at+36+count*12:at+36+count*20];actual=raw[at+36+count*20:at+60+count*20];at+=60+count*20
  u.mem_write(B,plane+bytes(64));u.mem_write(B+0x40,w(EDGE));u.mem_write(VERT,vertices);u.mem_write(POINT,point);u.mem_write(OUT,b'\xa5'*8)
  for i in range(count):u.mem_write(EDGE+i*32,w(VERT+i*12)+coords[i*8:i*8+8]+bytes(8)+w(EDGE+(i+1)%count*32,EDGE+(i-1)%count*32)+bytes(4))
  u.reg_write(UC_X86_REG_ECX,B);matched=run(u,0x4e1ad0,(POINT,OUT,OUT+4))&255;uv=bytes(u.mem_read(OUT,8));want=w(0,matched)+uv
  color=w(0xa5a5a5a5)
  if not matched:status=-3;uv_misses+=1
  else:
   aa,bb=struct.unpack('<2f',uv);aa=math.fmod(aa,1);bb=math.fmod(bb,1);aa+=1 if aa<0 else 0;bb+=1 if bb<0 else 0
   if int(bb*4+.5)*4+int(aa*4+.5)>=16:status=-4;bounds+=1
   else:
    trace=[];run(u,0x55cfa0,(17,*struct.unpack('<2I',uv),OUT));assert trace==['lock','release'];color=bytes(u.mem_read(OUT,4));status=0
  want+=w(status)+color;assert actual==want,(level['file'],index,'PC',actual.hex(),want.hex())
  face=plane+w(0,0xffffffff)+bytes(28)+w(count);assert len(face)==56
  data=vertices+face+b''.join(w(i)+coords[i*8:i*8+8] for i in range(count));x.mem_write(VERT,data);x.mem_write(B+0x1000,w(len(vertices)));g=bytearray(68)
  struct.pack_into('<I',g,0,VERT);struct.pack_into('<I',g,4,len(data));struct.pack_into('<I',g,20,count);struct.pack_into('<I',g,24,1);struct.pack_into('<I',g,56,B+0x1000);x.mem_write(B,bytes(g));x.mem_write(POINT,point);x.mem_write(WORK,w(B+0x4000,B+0x5000,256));x.mem_write(OUT,w(0xa5a5a5a5))
  got=w(run(x,entry,(B,0,POINT,IMAGE,WORK,OUT)))+bytes(x.mem_read(OUT,4));assert got==want[16:],(level['file'],index,'NXDK',got.hex(),want[16:].hex());assert bytes(x.mem_read(VERT,len(data)))==data
  if index==0:
   x.mem_write(WORK+8,w(count-1));x.mem_write(OUT,w(0xa5a5a5a5));assert w(run(x,entry,(B,0,POINT,IMAGE,WORK,OUT)))+bytes(x.mem_read(OUT,4))==w(-4,0xa5a5a5a5);guards+=1
  counted+=1;total+=1
 assert at==len(raw) and counted==level['faces'];results.append(dict(file=level['file'],faces=counted))
report=dict(result='PASS',faces=total,uv_misses=uv_misses,bounds_rejections=bounds,nxdk_workspace_guards=guards,levels=results,original_sha256=sha,scope='All faces in installed geometry audit levels: authored plane/corner positions/UV and centroid point. Full original4e1ad0 plus55cfa0 with only surface lock/release supplied, controlled4x4 RGBA pattern. PC authored adapter and compiled NXDK composed adapter on equivalent initial face records/swizzled texture match exactly. No game texture selection/animation or scene collision callback integration claim.')
(root/'artifacts/geometry-texture-sample.json').write_text(json.dumps(report,indent=2));print(report)
