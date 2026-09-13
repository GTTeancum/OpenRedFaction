"""VFX scale/quaternion/translation stages against original math helpers."""
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



rng=random.Random(0x559f50);inputs=[];responses=[]
for i in range(2048):
 data=f(*[rng.uniform(-100,100) for _ in range(9)]);inputs.append(data);x.mem_write(B,data);assert call('rf_vfx_face_normal',[B,OUT])==0;got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(FACE,bytes(0x90));o.mem_write(FACE+0x14,w(0,1,2));o.mem_write(STACK,w(STOP,FACE,B));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x559f50,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=bytes(o.mem_read(FACE+0x60,12));assert got==expected,(i,got.hex(),expected.hex());assert read(o,FACE+0x7c)&1
for data in (bytes(36),w(0x7fc00000)+bytes(32)):
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_face_normal',[B,OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==b'\xa5'*12;responses.append(w(status)+bytes(x.mem_read(OUT,12)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-face-normal'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=2,scope='Actual uncached559f50 with4fb050 cross and4faaf0 normalization, no hooks. Degenerate geometry rejected safely rather than original NaN. No cache, clipping, submission or native XEMU.')
(root/'artifacts/vfx-face-normal.json').write_text(json.dumps(report,indent=2));print(report)
