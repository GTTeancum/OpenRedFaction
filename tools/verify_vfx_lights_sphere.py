"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4d99c0);inputs=[];responses=[]
o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x5a38c8,b'\1');o.mem_write(0xc96874,w(123));o.mem_write(OWNER+0x1c8,w(123))
for i in range(2048):
 count=i%33;center=[rng.uniform(-10,10) for _ in range(3)];radius=rng.choice([0,1,5,10]);flags=[(i//33)%2,(i//66)%2];sources=[]
 for j in range(count):
  kind=(i+j)%6;pos=[rng.uniform(-20,20) for _ in range(3)];end=pos if j%3==0 else [rng.uniform(-20,20) for _ in range(3)];color=[0,0,0] if j%5==0 else [1,.5,.25];enabled=j%4;cls=j%2;lr=rng.choice([0,1,3,8])
  if j%7==0:
   center=[0,0,0];edge=radius+lr
   if edge>0:edge=struct.unpack('<f',w(struct.unpack('<I',f(edge))[0]+[0,-1,1][i%3]))[0]
   pos=[edge,0,0];end=pos
  sources.append(w(kind,0)+f(*pos,*end,0,0,0,*color,lr,0,0,1)+w(0)+bytes([enabled,cls,0,0]))
 data=w(count)+f(*center,radius)+w(*flags)+b''.join(sources);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*(4+4*count))
 assert call('rf_vfx_lights_sphere',[B+28,count,B+4,read(x,B+16),*flags,OUT+4,32,OUT])==0
 got=bytes(x.mem_read(OUT,4+4*count));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(OWNER+0x1bc,w(count,count,PTR))
 for j,s in enumerate(sources):
  p=FACE+j*0x90;o.mem_write(p,bytes(0x90));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4c,s[76:78]);o.mem_write(PTR+j*4,w(p))
 o.mem_write(STACK,w(STOP,OWNER,B+4,read(x,B+16),*flags));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4d99c0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 n=read(o,0xc9687c);expected=w(n)+w(*[(read(o,0xc4d588+j*4)-FACE)//0x90 for j in range(n)])+b'\xa5'*(4*(count-n))
 assert got==expected,(i,got.hex(),expected.hex())
# Invalid query, flag and nonfinite source preserve count and every index.
for data in [w(0)+f(0,0,0,-1)+w(1,1),w(0)+f(0,0,0,1)+w(2,1),w(1)+f(0,0,0,1)+w(1,1)+w(2,0)+f(float('nan'),*([0]*15))+w(0)+bytes([1,0,0,0])]:
 count=struct.unpack_from('<I',data)[0];inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*(4+4*count));status=call('rf_vfx_lights_sphere',[B+28,count,B+4,read(x,B+16),read(x,B+20),read(x,B+24),OUT+4,32,OUT]);assert status!=0 and bytes(x.mem_read(OUT,4+4*count))==b'\xa5'*(4+4*count);responses.append(w(status)+b'\xa5'*(4+4*count))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-lights-sphere'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=3,scope='Complete4d99c0 with actual4d96c0 cached-room fast path and509100 segment projection. Supplied cached list, no hooks. Stable ordering, class/enabled/color filters, unknown types, radius tangencies/adjacent floats and degenerate segments. Cache rebuild/global gates and native XEMU remain unverified.')
(root/'artifacts/vfx-lights-sphere.json').write_text(json.dumps(report,indent=2));print(report)
