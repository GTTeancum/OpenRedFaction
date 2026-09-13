"""Original special-lightmap corner normal traversal vs PC/NXDK."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_corner_normal\s+([0-9a-fA-F]+)',mp)[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f4192);inputs=[];responses=[]
VERTEX=B+0x8000;NODE=B+0x8100;TABLE=B+0x8200;FACES=B+0x9000
for i in range(4096):
 count=i%9;normal=[rng.uniform(-2,2) for _ in range(3)];base=f(*normal)+w(7,3);adjacent=[]
 for j in range(8):
  n=[rng.uniform(-2,2) for _ in range(3)] if j%3 else [-v for v in normal]
  adjacent.append(f(*n)+w(7 if j==i%8 else 20+j,0 if (i+j)%5==0 else 3))
 if i%4==0:adjacent[6]=adjacent[2]
 data=base+b''.join(adjacent)+w(count);inputs.append(data)
 o.mem_write(OWNER,bytes(80));o.mem_write(OWNER,base[:12]);o.mem_write(OWNER+60,w(3));o.mem_write(NODE,w(VERTEX));o.mem_write(VERTEX+32,w(count,count,TABLE))
 for j,face in enumerate(adjacent):
  identity,vertices=struct.unpack_from('<2I',face,12);p=OWNER if identity==7 else FACES+j*80
  if p!=OWNER:o.mem_write(p,bytes(80));o.mem_write(p,face[:12]);o.mem_write(p+60,w(vertices))
  o.mem_write(TABLE+j*4,w(p))
 o.mem_write(STACK,bytes(256));o.mem_write(STACK+28,w(OUT));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,NODE);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f)
 o.emu_start(0x4f4192,0x4f4222,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4222;expected=bytes(o.mem_read(STACK+32,12))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*12);assert call([B,B+20,count,OUT])==0;got=bytes(x.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex());responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-corner-normal'],input=b''.join(inputs))==b''.join(responses)
for bad in [f(0,0,0),f(float('nan'),1,0)]:
 x.mem_write(B,bad);x.mem_write(OUT,bytes([165])*12);assert call([B,B+20,0,OUT])!=0 and bytes(x.mem_read(OUT,12))==bytes([165])*12
report=dict(result='PASS',original_pc_nxdk_corners=4096,nxdk_guards=2,original_sha256=sha,x87_control_word='0x027f',scope='Original4f4192..4f4222 per-corner traversal with actual adjacency array access, dot/add/scale/normalize helpers, no hooks. Self/empty/opposed face exclusion, repeated adjacency and stored average. Outer allocation/topology construction and special sample selection excluded.')
(root/'artifacts/lightmap-corner-normal.json').write_text(json.dumps(report,indent=2));print(report)
