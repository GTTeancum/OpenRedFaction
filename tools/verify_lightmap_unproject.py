"""Replay full original4f24a0 and its actual axis helpers against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_unproject\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f24a0);inputs=[];responses=[]
for i in range(4096):
 normal=i%3;uaxis=[j for j in range(3) if j!=normal][(i//3)%2]
 scale=[rng.choice([-1,1])*2**rng.uniform(-12,12) for _ in range(2)];offset=[rng.uniform(-4,4) for _ in range(2)];uv=[rng.uniform(-4,4) for _ in range(2)]
 plane=[rng.uniform(-2,2) for _ in range(4)];plane[normal]=rng.choice([-1,1])*rng.uniform(.01,2)
 view=w(0,0,0xffffffff,0xffffffff)+f(*scale,*offset,*plane)+w(normal,uaxis);data=view+f(*uv)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis));o.mem_write(OWNER+0x6c,f(*plane));o.mem_write(OUT,bytes([165])*12)
 call(o,0x4f24a0,[OWNER,*struct.unpack('<II',f(*uv)),OUT]);expected=bytes(o.mem_read(OUT,12))
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*12);assert call(x,entry,[B,B+56,OUT])==0
 got=bytes(x.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex());inputs.append(data);responses.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-unproject'],input=b''.join(inputs))==b''.join(responses)
guards=[(16,f(0)),(20,f(float('inf'))),(24,f(float('nan'))),(32+normal*4,f(0)),(48,w(3)),(52,w(normal)),(56,f(float('inf'))),(60,f(float('nan')))]
for at,value in guards:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*12)
 status=call(x,entry,[B,B+56,OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==bytes([165])*12
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-unproject'],input=bytes(bad))==w(status)+bytes([165])*12
report=dict(result='PASS',original_pc_nxdk_points=len(inputs),pc_nxdk_guards=len(guards),original_sha256=sha,x87_control_word='0x027f',scope='Full unhooked original4f24a0 plus actual4e3f60/4e3fb0/4e4000, six axis orders, signed scales and arbitrary UVs. Image fields unused. Compiled NXDK CPU replay, not native rendered shadows; mask generation/ownership excluded.')
(root/'artifacts/lightmap-unproject.json').write_text(json.dumps(report,indent=2));print(report)
