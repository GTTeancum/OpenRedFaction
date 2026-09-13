"""Original414c69 fade block versus shared PC and compiled NXDK."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
B=0x30000000;S=B+0xe000;STOP=B+0xf000;OUT=B+0x1000;DRAW=OUT+8
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_glare_fade_samples\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
values=[0,0x80000000,f(-1),f(.01),f(.05),f(.0625)-1,f(.0625),f(.0625)+1,f(1/12)-1,f(1/12),f(1/12)+1,f(1),f(100),0x7f800000,0x7fc00000]
cases=[]
for view,a,b in itertools.product(range(2),values,values):
 samples=[f(.3),f(.7),f(2),f(3)];samples[view]=a;samples[2+view]=b;cases.append((view,*samples))
rng=random.Random(41469)
for n in range(512):cases.append((n%2,*[f(rng.uniform(-.1,1)) for _ in range(4)]))
original_count=len(cases);cases.append((2,f(1),f(2),f(3),f(4)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--glare-fade'],input=b''.join(w(*c) for c in cases));assert len(actual)==32*len(cases)
draws=0
for n,case in enumerate(cases):
 view,*samples=case
 if view<2:
  u.mem_write(B,bytes(768));u.mem_write(B+0x29c,w(*samples));u.mem_write(S+0x13,bytes([1]));u.mem_write(S+0x14,w(f(1)));u.mem_write(S+0x20,w(f(1)));u.mem_write(S+0x114,w(view))
  u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x414c69,0x414cef,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x414cef
  draw=u.mem_read(S+0x13,1)[0];draws+=draw
  want=w(0)+bytes(u.mem_read(B+0x29c,16))+bytes(u.mem_read(S+0x14,4))+bytes(u.mem_read(S+0x20,4))+w(draw)
 else:
  # RF_RANGE is checked independently from the original block's required view0/1.
  want=w(-4,*samples,f(1),f(1),0x12345678)
 x.mem_write(B,bytes(104));x.mem_write(B+24,w(*samples));x.mem_write(OUT,w(f(1),f(1),0x12345678))
 x.mem_write(S,w(STOP,B,view,OUT,DRAW));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=10000)
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+24,16))+bytes(x.mem_read(OUT,12))
 assert got==want and actual[n*32:(n+1)*32]==want,(n,case,got,want)
report={'result':'PASS','original_cases':original_count,'invalid_view_cases':1,'draws':draws,'original_sha256':sha,'scope':'Original414c69..414cef arithmetic/branches and stored sample changes versus PC/NXDK; both views, cutoff neighbors, signed zero, infinity and quiet NaN. Rounded output values exact. Common tail publication, upstream gates and drawing excluded.'}
(root/'artifacts/analysis/glare-fade.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
