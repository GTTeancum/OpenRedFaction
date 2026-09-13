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



rng=random.Random(0x518360);inputs=[];responses=[];counts={'rejected':0,'visible':0}
for i in range(2048):
 vertices=f(*[rng.uniform(-8,8) for _ in range(9)]);origin=f(*[rng.uniform(-2,2) for _ in range(3)]);mode=i%2;enabled=(i//2)%2;far=(i//4)%2;material=rng.randrange(-1,20)
 matrix=f(*rng.choice([(1,0,0,0,1,0,0,0,1),(0,0,.5,0,.25,0,-.125,0,0)]));camera=bytearray(328);camera[232:]=origin+matrix+f(.98)+w(mode)+w(enabled,mode,far)+f(1)+bytes(24);data=bytes(camera)+vertices+w(material);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*20);assert call('rf_vfx_world_face',[B,B+328,material&0xffffffff,OUT])==0;got=bytes(x.mem_read(OUT,20));responses.append(w(0)+got)
 o.mem_write(B,vertices);o.mem_write(FACE,bytes(0x90));o.mem_write(FACE+0x14,w(0,1,2,material));o.mem_write(FACE+0x84,w(CTX,CTX+44,CTX+88));o.mem_write(CTX,bytes(132));o.mem_write(0x1cd3840,w(0));o.mem_write(0x1c5e540,bytes(16));o.mem_write(0x17c7bcc,w(0x66));o.mem_write(0x17c7c18,w(0));o.mem_write(0x1818690,origin);o.mem_write(0x18186c8,matrix);o.mem_write(0x1818b7c,f(.98));o.mem_write(0x5a4d18,bytes([enabled,mode]));o.mem_write(0x1818b65,bytes([far]));o.mem_write(0x1818b6c,f(1));o.mem_write(0x1775b38,bytes(144))
 o.mem_write(STACK,w(STOP,FACE,B));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x554a80,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 visible=read(o,0x1cd3840);expected=bytes(o.mem_read(FACE+0x60,12))+bytes(o.mem_read(0x1c5e540,4))+w(visible);assert got==expected,(i,got.hex(),expected.hex());counts['visible' if visible else 'rejected']+=1
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-world-face'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,paths=counts,scope='Complete554a80 with empty projection cache and actual518360/518bf0 transforms and clip codes, normal, facing and biased depth. No hooks. Varied camera/modes/clip/far. No persistent cache, lighting/material submission or native XEMU.')
(root/'artifacts/vfx-world-face.json').write_text(json.dumps(report,indent=2));print(report)
