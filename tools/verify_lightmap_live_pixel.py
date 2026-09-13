"""Original class-light shading and packed addition vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_live_pixel\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

rng=random.Random(0x4f2e23);inputs=[];responses=[];SOURCE=B+0x3000;SHADE=B+0x5000
for a,v in [(0x5a38c4,0),(0x5a38cc,1),(0x1818b84,0)]:o.mem_write(a,w(v))
for i in range(2048):
 count=i%5;base=bytes([rng.randrange(256) for j in range(3)]+[0]);position=[rng.uniform(-3,3) for j in range(3)];normal=[rng.uniform(-1,1) for j in range(3)];sources=[]
 for j in range(4):sources.append(w(1+(i+j)%4,j%4)+f(*[rng.uniform(-4,4) for _ in range(6)],*[rng.uniform(-1,1) for _ in range(3)],*[rng.uniform(0,2) for _ in range(3)],rng.choice([1,5,20]),rng.random(),-.5,.75)+w(j%2))
 data=base+f(*position,*normal,.25)+w(count)+b''.join(sources);inputs.append(data);o.mem_write(B,data);o.mem_write(0xc9687c,w(count));o.mem_write(0x5a38e0,f(.25))
 for j,source in enumerate(sources):
  p=SOURCE+j*256;o.mem_write(p,bytes(256));o.mem_write(p+8,source[:4]);o.mem_write(p+12,source[8:44]);o.mem_write(p+0x38,source[60:64]);o.mem_write(p+0x3c,source[56:60]);o.mem_write(p+0x40,source[44:56]);o.mem_write(p+0x4e,source[72:73]);o.mem_write(p+0x54,source[4:8]);o.mem_write(p+0x84,source[64:72]);o.mem_write(0xc4d588+j*4,w(p))
 o.mem_write(SHADE,bytes(12));o.mem_write(STACK,w(STOP,SHADE,SHADE+4,SHADE+8,0,B+4,B+16,0xbf800000));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4daff0,STOP,count=100000)
 colors=bytes(o.mem_read(SHADE,12));o.mem_write(STACK-28,bytes(160));o.mem_write(STACK+0x14,colors[:4]);o.mem_write(STACK+0x3c,colors[4:8]);o.mem_write(STACK+0x40,colors[8:12]);o.mem_write(STACK+0x2c,w(OUT));o.mem_write(OWNER+12,w(IMAGE));o.mem_write(IMAGE+12,w(B));o.reg_write(UC_X86_REG_ESP,STACK-28);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBX,0);o.reg_write(UC_X86_REG_EBP,0);o.emu_start(0x4f2e23,0x4f2ea9,count=10000);expected=bytes(o.mem_read(OUT,2))
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5\xa5');assert call([B,B+4,B+16,0x3e800000,B+36,count,OUT])==0
 assert bytes(x.mem_read(OUT,2))==expected,(i,expected.hex(),bytes(x.mem_read(OUT,2)).hex());responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-live-pixel'],input=b''.join(inputs))==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_pixels=len(inputs),original_sha256=sha,x87_control_word='0x027f',scope='Full original4daff0 zero-ambient/negative-gain shading with actual4da8b0 plus unhooked4f2e23..4f2ea6 byte addition and1555 packing.0..4 mixed sources and saturated sums. Selected sources/position/normal supplied; selection, sampling, locks and dirty dispatch excluded.')
(root/'artifacts/lightmap-live-pixel.json').write_text(json.dumps(report,indent=2));print(report)
