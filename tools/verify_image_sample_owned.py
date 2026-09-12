"""Original55cfa0 bitmap sample with supplied lock/release versus PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_image_sample_owned\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
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
rng=random.Random(635);commands=[];answers=[];safe=0;bounds=0;packed_cases=0
for case in range(2048):
 width=(1,2,4,8)[case%4];height=(1,2,4,8)[case//4%4];fmt=(2,4,5,7,3,6)[case//16%6];packed=int(fmt==5 and case%2==0);bpp=1 if fmt==2 else 2 if fmt in (4,5) else 4;pitch=width*bpp
 uv=f(rng.choice((0,.99999,-.00001,1,-1,.25,rng.uniform(-8,8))),rng.choice((0,.99999,-.00001,1,-1,.25,rng.uniform(-8,8))));a,b=struct.unpack('<2f',uv)
 raw=bytes(rng.randrange(256) for _ in range(pitch*height));decoded=bytearray()
 for i in range(width*height):
  p=raw[i*bpp:(i+1)*bpp]
  if fmt==2:pixel=bytes((255,255,255,p[0]))
  elif fmt in (4,5):
   word=int.from_bytes(p,'little')
   if packed:pixel=p
   elif fmt==4:pixel=bytes((((word>>8)&15)*17,((word>>4)&15)*17,(word&15)*17,(word>>12)*17))
   else:pixel=bytes((((word>>10)&31)*255//31,((word>>5)&31)*255//31,(word&31)*255//31,255 if word&0x8000 else 0))
  elif fmt==7:pixel=bytes((p[2],p[1],p[0],p[3]))
  else:pixel=bytes(4)
  decoded+=pixel
 aa=math.fmod(a,1);bb=math.fmod(b,1);aa+=1 if aa<0 else 0;bb+=1 if bb<0 else 0;index=int(bb*height+.5)*width+int(aa*width+.5)
 if fmt in (2,4,5,7) and index>=width*height:want=w(-4,0xa5a5a5a5);bounds+=1
 else:
  trace=[];u.mem_write(DATA,raw+bytes(512-len(raw)));u.mem_write(OUT,w(0xa5a5a5a5));run(u,0x55cfa0,(17,*struct.unpack('<2I',uv),OUT));assert trace==['lock','release'];want=w(0)+bytes(u.mem_read(OUT,4));safe+=1
 commands.append(w(width,height,fmt,packed)+uv+bytes(decoded)+bytes(256-len(decoded)));answers.append(want)
 swizzled=bytearray(len(decoded));stride=2 if packed else 4
 for yy in range(height):
  for xx in range(width):
   address=0;bit=0
   for level in range(max(width.bit_length(),height.bit_length())-1):
    if (1<<level)<width:address|=((xx>>level)&1)<<bit;bit+=1
    if (1<<level)<height:address|=((yy>>level)&1)<<bit;bit+=1
   swizzled[address*stride:(address+1)*stride]=decoded[(yy*width+xx)*stride:(yy*width+xx+1)*stride]
 x.mem_write(B,w(width,height,len(decoded),fmt,DATA));x.mem_write(DATA,bytes(swizzled));x.mem_write(OUT,w(0xa5a5a5a5));actual=w(run(x,entry,(B,*struct.unpack('<2I',uv),OUT)))+bytes(x.mem_read(OUT,4));assert actual==want,(case,actual.hex(),want.hex());assert bytes(x.mem_read(DATA,len(decoded)))==swizzled;packed_cases+=packed
guards=((3,2,0.,0.,-2),(0,2,0.,0.,-4),(2,2,math.nan,0.,-2),(2,2,0.,math.inf,-2))
for ww,hh,aa,bb,status in guards:
 commands.append(w(ww,hh,7,0)+f(aa,bb)+bytes(256));want=w(status,0xa5a5a5a5);answers.append(want)
 x.mem_write(B,w(ww,hh,ww*hh*4,7,DATA));x.mem_write(OUT,w(0xa5a5a5a5));actual=w(run(x,entry,(B,*struct.unpack('<2I',f(aa,bb)),OUT)))+bytes(x.mem_read(OUT,4));assert actual==want
pc=subprocess.check_output([str(root/'build/pc/Release/rf_image_probe.exe'),'--sample-owned'],input=b''.join(commands));assert pc==b''.join(answers),'PC mismatch'
assert safe and bounds and packed_cases
report=dict(result='PASS',cases=2048,port_guards=len(guards),original_safe_reads=safe,bounds_rejections=bounds,packed1555_cases=packed_cases,original_sha256=sha,scope='Owned decoded PC row-major and actual compiled NXDK swizzled sampler compared to full original55cfa0 locked native-format source. Only original lock/release supplied. Formats2/4/5/7 plus transparent fallbacks, normalized4444/1555 and packed1555, rectangular power-of-two sizes and row crossings. Outside logical allocations rejected without executing unsafe original reads; pixels unchanged. No extra texture allocation in adapter. Authored asset composition and native XEMU scene binding remain open.')
(root/'artifacts/image-sample-owned.json').write_text(json.dumps(report,indent=2));print(report)
