"""Unhooked original ordinary lightmap sampling and accumulation vs PC/NXDK."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_accumulate_samples\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
o.mem_map(0,4096);rng=random.Random(0x4f3390);inputs=[];responses=[];pixels=0;changed=0
SOURCE=B+0x3000;MASK=B+0x7800
o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x1818b84,w(0))
for i in range(512):
 width=1+i%8;height=1+(i//8)%8;count=i%5;masked=i%2;normal=i%3;uaxis=[j for j in range(3) if j!=normal][i%2]
 scale=[rng.uniform(.2,2) for _ in range(2)];offset=[rng.uniform(-1,1) for _ in range(2)];plane=[rng.uniform(-1,1) for _ in range(4)];plane[normal]=rng.uniform(.2,1)
 sample=w(16,16,2,3)+f(*scale,*offset,*plane)+w(normal,uaxis);sources=[]
 for j in range(4):sources.append(w(1+(i+j)%4,j%4)+f(*[rng.uniform(-4,4) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],*[rng.uniform(0,.5) for _ in range(3)],rng.choice([1,5,20]),rng.random(),-.5,.75)+w(j%2))
 channels=[f(*[rng.uniform(-.05,.1) for _ in range(64)]) for j in range(3)];masks=[bytes(rng.choice([0,1,32,127,255]) for _ in range(64)) for j in range(4)]
 data=sample+w(width,height,count,masked)+f(.25)+b''.join(sources)+b''.join(channels)+b''.join(masks);inputs.append(data)
 o.mem_write(B,data);o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(IMAGE,2,3,width,height));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis));o.mem_write(OWNER+0x6c,f(*plane));o.mem_write(IMAGE,w(0,16,16,OUT));o.mem_write(0x5a38e0,f(.25));o.mem_write(0xc9687c,w(count))
 for j,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,channels[j])
 for j,s in enumerate(sources):
  p=SOURCE+j*256;o.mem_write(p,bytes(256));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x38,s[60:64]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4e,s[72:73]);o.mem_write(p+0x54,s[4:8]);o.mem_write(p+0x84,s[64:72]);o.mem_write(0xc4d588+j*4,w(p));o.mem_write(MASK+j*4,w(B+1148+j*64))
 o.mem_write(STACK,w(STOP,0,OWNER,0,MASK if masked else 0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f3390,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=b''.join(bytes(o.mem_read(a,256)) for a in [0x1431de0,0x14b23e0,0x13f1de0]);pixels+=width*height;changed+=expected!=b''.join(channels)
 x.mem_write(B,data);x.mem_write(MASK,w(*(B+1148+j*64 for j in range(4))))
 x.mem_write(VIEW,sample+w(width,height,B+76,count,MASK if masked else 0,64)+f(.25)+w(B+380,B+636,B+892,64))
 assert call([VIEW])==0;got=bytes(x.mem_read(B+380,768));assert got==expected,(i,width,height);responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-accumulate-grid'],input=b''.join(inputs))==b''.join(responses)
for offset,value in [(68,65),(76,0),(96,0),(84,0)]:
 before=bytes(x.mem_read(B+380,768));saved=bytes(x.mem_read(VIEW+offset,4));x.mem_write(VIEW+offset,w(value))
 assert call([VIEW])!=0 and bytes(x.mem_read(B+380,768))==before;x.mem_write(VIEW+offset,saved)
report=dict(result='PASS',original_pc_nxdk_grids=512,nxdk_binding_guards=4,pixels=pixels,changed_grids=changed,original_sha256=sha,x87_control_word='0x027f',scope='Complete original4f3390 ordinary path including actual4da8b0, plane routines and geometry/falloff; no hooks. Selected sources/masks/mapping/initial planes supplied.0-4 mixed sources, all axis orders, softening, clamping and untouched tail. Special polygon sampling, mask generation and rendering excluded.')
(root/'artifacts/lightmap-accumulate-grid.json').write_text(json.dumps(report,indent=2));print(report)
