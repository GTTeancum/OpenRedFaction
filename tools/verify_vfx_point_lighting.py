"""Complete original scene-light shading for ordered point sources."""
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
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



rng=random.Random(0x4da8b0);inputs=[];responses=[]
o.mem_write(0x5a38c4,w(0));o.mem_write(0x5a38cc,w(1));o.mem_write(0x1818b84,w(0))
for i in range(1024):
 count=i%17;position=[rng.uniform(-5,5) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)];ambient=[rng.random()*.25 for _ in range(3)];sources=[]
 for j in range(count):sources.append(f(*[rng.uniform(-10,10) for _ in range(3)],*[rng.random()*2 for _ in range(3)],rng.choice([0,1,5,20]))+w(j%4))
 data=w(count)+f(*position,*normal,*ambient)+b''.join(sources);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*3);assert call('rf_vfx_point_lighting',[B+4,B+16,B+28,B+40,count,OUT])==0;got=bytes(x.mem_read(OUT,3));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(0x5a38d4,data[28:40]);o.mem_write(0xc9687c,w(count))
 for j,source in enumerate(sources):
  pointer=FACE+j*0x90;o.mem_write(pointer,bytes(0x90));o.mem_write(pointer+8,w(2));o.mem_write(pointer+12,source[:12]);o.mem_write(pointer+0x3c,source[24:28]);o.mem_write(pointer+0x40,source[12:24]);o.mem_write(pointer+0x54,source[28:32]);o.mem_write(0xc4d588+j*4,w(pointer))
 o.mem_write(STACK,w(STOP,OUT,OUT+1,OUT+2,1,B+4,B+16,0x40000000));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4daff0,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,3)),(i,got.hex(),bytes(o.mem_read(OUT,3)).hex())
for data in [w(0)+f(*([0]*6+[2,0,0])),w(1)+f(*([0]*9))+f(*([0]*6+[1]))+w(4)]:
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*3);status=call('rf_vfx_point_lighting',[B+4,B+16,B+28,B+40,read(x,B),OUT]);assert status!=0 and bytes(x.mem_read(OUT,3))==b'\xa5'*3;responses.append(w(status)+b'\xa5'*3)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-point-lighting'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=1024,guards=2,scope='Complete4daff0+4da8b0, real point geometry/falloff/accumulation/conversion and zero-to16 active point lights. Supplied active light table and ambient; no hooks. Mixed light ownership/native XEMU remain.')
(root/'artifacts/vfx-point-lighting.json').write_text(json.dumps(report,indent=2));print(report)
