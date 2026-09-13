"""Original scene-light RGB normalization, gain and byte conversion."""
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



rng=random.Random(0x4daff0);inputs=[];responses=[]
for i in range(4096):
 rgb=[rng.uniform(0,3 if i%2 else 1) for _ in range(3)];ambient=[rng.random() for _ in range(3)];gain=rng.choice([-1,0,.25,.5,1,2,4]);data=f(*rgb,*ambient,gain);inputs.append(data);words=struct.unpack('<7I',data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*3);assert call('rf_vfx_light_rgb',[B,B+12,words[6],OUT])==0;got=bytes(x.mem_read(OUT,3));responses.append(w(0)+got)
 o.mem_write(STACK,w(words[1],words[2],STOP,OUT,OUT+1,OUT+2,words[0],0,0,words[6]));o.mem_write(0x5a38d4,data[12:24]);o.reg_write(UC_X86_REG_ECX,words[0]);o.reg_write(UC_X86_REG_EAX,words[1]);o.reg_write(UC_X86_REG_EDX,words[2]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4db065,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,3)),(i,got.hex(),bytes(o.mem_read(OUT,3)).hex())
for data in [f(-1,0,0,0,0,0,2),f(0,0,0,2,0,0,2),w(0x7fc00000)+f(0,0,0,0,0,2)]:
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*3);status=call('rf_vfx_light_rgb',[B,B+12,struct.unpack_from('<I',data,24)[0],OUT]);assert status!=0 and bytes(x.mem_read(OUT,3))==b'\xa5'*3;responses.append(w(status)+b'\xa5'*3)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-light-rgb'],input=b''.join(inputs));assert pc==b''.join(responses),[(i,inputs[i].hex(),pc[i*7:i*7+7].hex(),v.hex()) for i,v in enumerate(responses) if pc[i*7:i*7+7]!=v][:5]
report=dict(result='PASS',original_pc_nxdk_cases=4096,guards=3,scope='Original4db065..4db1a7 conversion with actual min/max and ftol helpers; accumulated RGB supplied after scene-light query. No hooks or native XEMU.')
(root/'artifacts/vfx-light-rgb.json').write_text(json.dumps(report,indent=2));print(report)
