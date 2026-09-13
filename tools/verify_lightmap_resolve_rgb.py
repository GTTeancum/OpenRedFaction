"""Original4f26a0 complete RGB resolve loop and actual callees vs PC/NXDK."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_resolve_rgb\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
counts=[0,0]
def observe(u,a,size,ctx):counts[a==0x4f3100]+=1
o.hook_add(UC_HOOK_CODE,observe,begin=0x4f3040,end=0x4f3040);o.hook_add(UC_HOOK_CODE,observe,begin=0x4f3100,end=0x4f3100)
rng=random.Random(0x4f2acf);inputs=[];responses=[];pixels=0
for i in range(512):
 width=1+i%16;height=1+(i//16)%16;px=rng.randrange(4);py=rng.randrange(4);pitch=(width+px+rng.randrange(4))*3;offset=py*pitch+px*3;dirty=rng.randrange(256);channels=[f(*[rng.uniform(-2,4) for _ in range(256)]) for c in range(3)]
 data=w(width,height,pitch,offset,dirty)+b''.join(channels);inputs.append(data);initial=bytes([165])*1536
 for c,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,channels[c])
 o.mem_write(OUT,initial);o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+8,bytes([dirty]));o.mem_write(OWNER+12,w(IMAGE,px,py,width,height));o.mem_write(IMAGE,w(0,pitch//3,32,OUT))
 o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBP,0);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f)
 previous=counts.copy();o.emu_start(0x4f2acf,0x4f2c74,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f2c74
 filtered=(width-4)*(height-4) if width>=9 and height>=9 else 0;assert [counts[j]-previous[j] for j in range(2)]==[width*height-filtered,filtered]
 expected=bytes(o.mem_read(OWNER+8,1))+bytes(o.mem_read(OUT,1536));pixels+=width*height
 x.mem_write(B,data);x.mem_write(OUT,initial);x.mem_write(DIRTY,bytes([dirty]));x.mem_write(VIEW,w(B+20,B+1044,B+2068,256,width,height))
 assert call([VIEW,OUT+offset,1536-offset,pitch,DIRTY])==0;got=bytes(x.mem_read(DIRTY,1))+bytes(x.mem_read(OUT,1536));assert got==expected,(i,width,height);responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-resolve-rgb'],input=b''.join(inputs))==b''.join(responses)
# Short output and malformed dimensions reject before any writes.
for size,stride in [(1,pitch),(1536-offset,1)]:
 before=bytes(x.mem_read(OUT,1536))+bytes(x.mem_read(DIRTY,1));assert call([VIEW,OUT+offset,size,stride,DIRTY])!=0;assert bytes(x.mem_read(OUT,1536))+bytes(x.mem_read(DIRTY,1))==before
report=dict(result='PASS',original_pc_nxdk_mappings=512,pixels=pixels,direct_calls=counts[0],filtered_calls=counts[1],nxdk_buffer_guards=2,original_sha256=sha,x87_control_word='0x027f',scope='Original4f2acf..4f2c74 resolve loop including actual4f3040/4f3100/CRT; accumulated planes and caller frame supplied. Every dimension pair1..16 twice, image offsets/padding and dirty bit8. Source accumulation, bitmap locking and native rendering excluded.')
(root/'artifacts/lightmap-resolve-rgb.json').write_text(json.dumps(report,indent=2));print(report)
