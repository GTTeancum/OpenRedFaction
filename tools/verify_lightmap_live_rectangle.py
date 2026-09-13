"""Original live class-light rectangle sampling/shading/packing vs PC/NXDK."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_live_rectangle\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

rng=random.Random(0x4f2cfe);inputs=[];responses=[];pixels=0;SOURCE=B+0x3000;UPLOAD=B+0x7800
for a,v in [(0x5a38c4,0),(0x5a38cc,1),(0x1818b84,0)]:o.mem_write(a,w(v))
for i in range(256):
 width=1+i%8;height=1+(i//8)%8;count=1+i%4;pitch=32+(i%3)*2;dirty=i%256;normal=i%3;uaxis=[j for j in range(3) if j!=normal][i%2]
 scale=[rng.uniform(.2,2) for j in range(2)];offset=[rng.uniform(-1,1) for j in range(2)];plane=[rng.uniform(-1,1) for j in range(4)];plane[normal]=rng.uniform(.2,1)
 sample=w(16,16,2,3)+f(*scale,*offset,*plane)+w(normal,uaxis);sources=[]
 for j in range(4):sources.append(w(1+(i+j)%4,j%4)+f(*[rng.uniform(-4,4) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],*[rng.uniform(0,2) for _ in range(3)],rng.choice([1,5,20]),rng.random(),-.5,.75)+w(j%2))
 rgb=bytes(rng.randrange(256) for j in range(768));data=sample+w(width,height,count,pitch,dirty)+b''.join(sources)+rgb;inputs.append(data)
 o.mem_write(B,data);o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+8,bytes([dirty,i%2]));o.mem_write(OWNER+12,w(IMAGE,2,3,width,height));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis));o.mem_write(OWNER+0x6c,f(*plane));o.mem_write(IMAGE,w(0,16,16,B+380));o.mem_write(0x5a38e0,f(.25));o.mem_write(0xc9687c,w(count));o.mem_write(OUT,bytes([165])*768)
 for j,source in enumerate(sources):
  p=SOURCE+j*256;o.mem_write(p,bytes(256));o.mem_write(p+8,source[:4]);o.mem_write(p+12,source[8:44]);o.mem_write(p+0x38,source[60:64]);o.mem_write(p+0x3c,source[56:60]);o.mem_write(p+0x40,source[44:56]);o.mem_write(p+0x4e,source[72:73]);o.mem_write(p+0x54,source[4:8]);o.mem_write(p+0x84,source[64:72]);o.mem_write(0xc4d588+j*4,w(p))
 o.mem_write(STACK,bytes(160));o.mem_write(STACK+0x5c,w(OUT));o.mem_write(STACK+0x68,w(pitch));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f2cfe,0x4f2ef3,count=1000000)
 assert o.reg_read(UC_X86_REG_EIP)==0x4f2ef3;expected=bytes(o.mem_read(OUT,768));changed=bytes(o.mem_read(OWNER+8,1));assert bytes(o.mem_read(B+380,768))==rgb
 x.mem_write(B,data);x.mem_write(VIEW,sample+w(width,height,B+76,count,0,0)+f(.25)+bytes(16));x.mem_write(UPLOAD,w(B+380,768,48,OUT,768,pitch,2,3,width,height));x.mem_write(OUT,bytes([165])*768);x.mem_write(DIRTY,bytes([dirty]))
 assert call([VIEW,UPLOAD,DIRTY])==0 and bytes(x.mem_read(OUT,768))==expected and bytes(x.mem_read(DIRTY,1))==changed,i
 assert bytes(x.mem_read(B+380,768))==rgb;responses.append(w(0)+changed+expected);pixels+=width*height
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-live-rectangle'],input=b''.join(inputs))==b''.join(responses)
for offset,value in [(4,1),(16,1),(20,1),(32,0)]:
 saved=bytes(x.mem_read(UPLOAD+offset,4));x.mem_write(UPLOAD+offset,w(value));x.mem_write(OUT,bytes([165])*768);x.mem_write(DIRTY,b'\x09')
 assert call([VIEW,UPLOAD,DIRTY])!=0 and bytes(x.mem_read(OUT,768))==bytes([165])*768 and bytes(x.mem_read(DIRTY,1))==b'\x09';x.mem_write(UPLOAD+offset,saved)
report=dict(result='PASS',original_pc_nxdk_rectangles=len(inputs),pixels=pixels,nxdk_guards=4,original_sha256=sha,x87_control_word='0x027f',scope='Unhooked original4f2cfe..4f2ef0 complete locked rectangle with actual plane coordinates,4daff0/4da8b0,1555 addition and dirty8 clear;1..4 mixed sources, all axis orders, special flag0/1, row padding and unchanged base RGB. Selection, lock/unlock and final dirty reset excluded.')
(root/'artifacts/lightmap-live-rectangle.json').write_text(json.dumps(report,indent=2));print(report)
