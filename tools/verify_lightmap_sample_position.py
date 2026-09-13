"""Original ordinary lightmap sample grids and plane reconstruction vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_sample_position\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
o.mem_map(0,4096);observed=[]
def capture(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ptr=struct.unpack('<I',u.mem_read(sp+16,4))[0];observed.append(bytes(u.mem_read(ptr,12)))
 u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0]);u.reg_write(UC_X86_REG_ESP,sp+4)
o.hook_add(UC_HOOK_CODE,capture,begin=0x4da8b0,end=0x4da8b0)
rng=random.Random(0x4f3f51);inputs=[];responses=[];points=0
for i in range(384):
 width=1+i%8;height=1+(i//8)%8;iw=width+rng.randrange(256);ih=height+rng.randrange(256);ox=rng.randrange(iw-width+1);oy=rng.randrange(ih-height+1)
 normal=i%3;uaxis=[j for j in range(3) if j!=normal][i%2];scale=[rng.choice([-1,1])*rng.uniform(.1,4) for _ in range(2)];offset=[rng.uniform(-1,1) for _ in range(2)];plane=[rng.uniform(-2,2) for _ in range(4)];plane[normal]=rng.choice([-1,1])*rng.uniform(.1,2)
 view=w(iw,ih,ox,oy)+f(*scale,*offset,*plane)+w(normal,uaxis)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(IMAGE,ox,oy,width,height));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis));o.mem_write(OWNER+0x6c,f(*plane));o.mem_write(IMAGE,w(0,iw,ih,OUT))
 observed.clear();o.mem_write(STACK,w(STOP,0,OWNER,0,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f3390,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP and len(observed)==width*height
 for y in range(height):
  for xx in range(width):
   data=view+w(xx,y);inputs.append(data);expected=observed[y*width+xx];x.mem_write(B,data);x.mem_write(OUT,bytes([165])*12)
   assert call([B,xx,y,OUT])==0;got=bytes(x.mem_read(OUT,12));assert got==expected,(i,xx,y,got.hex(),expected.hex());responses.append(w(0)+expected);points+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-sample-position'],input=b''.join(inputs))==b''.join(responses)
# Invalid divisors, axes and out-of-image origins preserve output.
for at,value in [(0,w(0)),(16,f(0)),(24,f(float('nan'))),(32+normal*4,f(0)),(48,w(3)),(52,w(normal)),(8,w(0xffffffff))]:
 saved=bytes(x.mem_read(B+at,4));x.mem_write(B+at,value);x.mem_write(OUT,bytes([165])*12)
 assert call([B,0,0,OUT])!=0 and bytes(x.mem_read(OUT,12))==bytes([165])*12;x.mem_write(B+at,saved)
report=dict(result='PASS',original_maps=384,original_pc_nxdk_points=points,nxdk_guards=7,original_sha256=sha,x87_control_word='0x027f',scope='Complete original4f3390 ordinary sample traversal and actual axis reconstruction table; only4da8b0 accumulator supplied to record points. All six axis orders, image/mapping offsets and signed scales. Special polygon sampling, source accumulation and renderer lifecycle excluded.')
(root/'artifacts/lightmap-sample-position.json').write_text(json.dumps(report,indent=2));print(report)
