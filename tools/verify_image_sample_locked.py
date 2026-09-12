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
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_image_sample_locked\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
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
rng=random.Random(0x55cfa0);commands=[];answers=[];formats={};crossings=0
for case in range(3072):
 width=1+case%8;height=1+(case//8)%8;fmt=case%10;bpp=1 if fmt==2 else 2 if fmt in (4,5) else 4;pitch=width*bpp+(case%4)*4
 coords=(0,1,-1,.99999,-.00001,.5,2.75,-3.25,1e20,-1e20)
 a=coords[case%10] if case%3 else rng.uniform(-8,8);b=coords[(case//10)%10] if case%5 else rng.uniform(-8,8)
 pixels=bytes(rng.randrange(256) for _ in range(512));uv=f(a,b);a,b=struct.unpack('<2f',uv);wire=w(width,height,pitch,fmt,512)+uv+pixels
 commands.append(wire);trace=[];u.mem_write(DATA,pixels);u.mem_write(OUT,w(0xa5a5a5a5));run(u,0x55cfa0,(17,*struct.unpack('<2I',uv),OUT));assert trace==['lock','release'];want=w(0)+bytes(u.mem_read(OUT,4));answers.append(want)
 x.mem_write(B,w(width,height,pitch,fmt,512,DATA));x.mem_write(DATA,pixels);x.mem_write(OUT,w(0xa5a5a5a5));actual=w(run(x,entry,(B,*struct.unpack('<2I',uv),OUT)))+bytes(x.mem_read(OUT,4));assert actual==want,(case,actual.hex(),want.hex(),a,b)
 assert bytes(x.mem_read(DATA,512))==pixels and bytes(u.mem_read(DATA,512))==pixels
 formats[fmt]=formats.get(fmt,0)+1
 aa=math.fmod(a,1);bb=math.fmod(b,1);aa=aa+1 if aa<0 else aa;bb=bb+1 if bb<0 else bb
 crossings+=int(aa*width+.5)>=width or int(bb*height+.5)>=height
# Port bounds: original would access outside the provided allocation. Do not run those original reads.
base=bytearray(commands[7]);struct.pack_into('<5I',base,0,2,2,8,7,16);struct.pack_into('<2f',base,20,.99999,.99999)
guards=[(bytes(base),-4)]
for offset,value,status in ((0,0,-4),(4,0,-4),(8,0,-4),(16,0,-4),(20,math.nan,-2),(24,math.inf,-2)):
 wire=bytearray(commands[7]);struct.pack_into('<f' if offset in (20,24) else '<I',wire,offset,value);guards.append((bytes(wire),status))
for wire,status in guards:
 width,height,pitch,fmt,size=struct.unpack_from('<5I',wire);x.mem_write(B,w(width,height,pitch,fmt,size,DATA));x.mem_write(DATA,wire[28:]);x.mem_write(OUT,w(0xa5a5a5a5));actual=w(run(x,entry,(B,*struct.unpack_from('<2I',wire,20),OUT)))+bytes(x.mem_read(OUT,4));want=w(status,0xa5a5a5a5);assert actual==want
 commands.append(wire);answers.append(want)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_image_probe.exe'),'--sample-locked'],input=b''.join(commands));assert pc==b''.join(answers),'PC mismatch'
assert crossings
report=dict(result='PASS',original_cases=3072,port_guards=len(guards),formats=formats,rounded_dimension_crossings=crossings,original_sha256=sha,scope='Full original55cfa0 with actual remainder, float-to-int and color extraction; only lock55ce00 and release50e310 supplied. PC/NXDK exact RGBA, negative/repeated UV, signed zero/integer/large coordinates, pitched buffers, rounded row/padding crossings and formats0..9. Format2 alpha8,4 ARGB4444,5 ARGB1555,7 ARGB8888; others transparent white. Bounds failures preserve color. Original37f/NXDK27f preserved. No live renderer lock, swizzled image adapter or native scene claim.')
(root/'artifacts/image-sample-locked.json').write_text(json.dumps(report,indent=2));print(report)
