"""Original cone-light geometry and optional softened normal."""
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



rng=random.Random(0x4daf30);inputs=[];responses=[]
for i in range(2048):
 position=[rng.uniform(-10,10) for _ in range(3)];normal=[rng.uniform(-1,1) for _ in range(3)];light=position if i%7==0 else [rng.uniform(-10,10) for _ in range(3)];radius=rng.choice([0,1,5,30]);soften=i%2;axis=[rng.uniform(-1,1) for _ in range(3)];data=f(*(position+normal+light+axis+[radius]))+w(soften);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);assert call('rf_vfx_cone_light',[B,B+12,B+24,B+36,struct.unpack_from('<I',data,48)[0],soften,OUT])==0;got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(CTX,b'\0');o.mem_write(STACK,w(STOP,B,B+12,B+24,B+36,struct.unpack_from('<I',data,48)[0],CTX if soften else 0,OUT,OUT+4,OUT+8));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x4daf30,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert got==bytes(o.mem_read(OUT,12)),(i,got.hex(),bytes(o.mem_read(OUT,12)).hex())
for data in [f(*([0]*12+[-1]))+w(0),w(0x7fc00000)+f(*([0]*11+[1]))+w(0),f(*([0]*12+[1]))+w(2)]:
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_cone_light',[B,B+12,B+24,B+36,struct.unpack_from('<I',data,48)[0],struct.unpack_from('<I',data,52)[0],OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==b'\xa5'*12;responses.append(w(status)+b'\xa5'*12)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-cone-light'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=3,scope='Complete4daf30 with actual vector difference/normalization/scale/dot helpers, no hooks. Both optional branches, coincident fallback and radius rejection. No light accumulator/native XEMU.')
(root/'artifacts/vfx-cone-light.json').write_text(json.dumps(report,indent=2));print(report)
