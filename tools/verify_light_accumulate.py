"""Raw original mixed-light accumulation including visibility weights."""
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
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4da8b0);inputs=[];responses=[];weighted_cases=0
MASK=B+0x8000;WEIGHTS=B+0x8100
o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x1818b84,w(0))
for i in range(4096):
 count=i%17;weighted=i%2;soften=(i//2)%2;weighted_cases+=weighted;position=[rng.uniform(-5,5) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)];initial=[rng.random()*.1 for _ in range(3)];sources=[]
 weights=bytes(rng.choice([0,1,32,127,254,255]) for _ in range(16))
 for j in range(16):
  sources.append(w(1+(i+j)%4,j%4)+f(*[rng.uniform(-10,10) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],*[rng.random()*.5 for _ in range(3)],rng.choice([0,1,5,20]),rng.random(),-.5,.75)+w(j%2))
 data=w(count,weighted|(soften<<1))+f(*position,*normal,*initial,.25)+b''.join(sources)+weights;inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12)
 assert call('rf_vfx_light_accumulate',[B+8,B+20,B+32,read(x,B+44),B+48,count,B+1264 if weighted else 0,soften,OUT])==0
 got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(OUT,data[32:44]);o.mem_write(0x5a38e0,data[44:48]);o.mem_write(0xc9687c,w(count))
 o.mem_write(WEIGHTS,weights);o.mem_write(WEIGHTS+32,b"\x00")
 for j,s in enumerate(sources[:count]):
  p=FACE+j*0x90;o.mem_write(p,bytes(0x90));o.mem_write(p+8,s[:4]);o.mem_write(p+12,s[8:44]);o.mem_write(p+0x38,s[60:64]);o.mem_write(p+0x3c,s[56:60]);o.mem_write(p+0x40,s[44:56]);o.mem_write(p+0x4e,s[72:73]);o.mem_write(p+0x54,s[4:8]);o.mem_write(p+0x84,s[64:72]);o.mem_write(0xc4d588+j*4,w(p));o.mem_write(MASK+j*4,w(WEIGHTS+j))
 o.mem_write(STACK,w(STOP,OUT,OUT+4,OUT+8,B+8,B+20,MASK if weighted else 0,0,WEIGHTS+32 if soften else 0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4da8b0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 if got!=bytes(o.mem_read(OUT,12)):(root/'artifacts/light-accumulate-failure.bin').write_bytes(data)
 assert got==bytes(o.mem_read(OUT,12)),(i,got.hex(),bytes(o.mem_read(OUT,12)).hex())
 x.mem_write(B+32,data[32:44]);assert call('rf_vfx_light_accumulate',[B+8,B+20,B+32,read(x,B+44),B+48,count,B+1264 if weighted else 0,soften,B+32])==0;assert bytes(x.mem_read(B+32,12))==got
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-accumulate'],input=b''.join(inputs))==b''.join(responses)
# Transaction guard and zero-weight skip of an otherwise invalid source.
x.mem_write(B+48,w(99));x.mem_write(B+1264,b'\xff');x.mem_write(OUT,b'\xa5'*12)
assert call('rf_vfx_light_accumulate',[B+8,B+20,B+32,read(x,B+44),B+48,1,B+1264,0,OUT])!=0
assert bytes(x.mem_read(OUT,12))==b'\xa5'*12
x.mem_write(B+1264,b'\x00')
assert call('rf_vfx_light_accumulate',[B+8,B+20,B+32,read(x,B+44),B+48,1,B+1264,0,OUT])==0
assert bytes(x.mem_read(OUT,12))==bytes(x.mem_read(B+32,12))
report=dict(result='PASS',original_pc_nxdk_cases=4096,nxdk_error_guards=1,nxdk_zero_weight_skip=1,x87_control_word="0x027f",weighted_cases=weighted_cases,in_place_nxdk_cases=4096,scope='Complete original4da8b0 with unhooked geometry/falloff/float accumulation.0-16 mixed sources, supplied normal, optional per-light visibility bytes, initial RGB and active list. Both softened and unsoftened point/cone geometry. Mask generation, disabled-light gate and source selection excluded.')
(root/'artifacts/light-accumulate.json').write_text(json.dumps(report,indent=2));print(report)
