"""Original VFX effect-time and normalized bitmap frame clocks."""
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



state=dict(count=0,duration=0)
def bitmap(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);assert read(u,sp+4)==100;u.mem_write(read(u,sp+8),w(state['duration']));u.reg_write(UC_X86_REG_EAX,state['count']);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4)
o.hook_add(UC_HOOK_CODE,bitmap,begin=0x50f380,end=0x50f380)
rng=random.Random(0x54a630);inputs=[];responses=[]
for i in range(4096):
 count=rng.randrange(1,65);duration=rng.choice([.1,.25,1,2,10]);start=rng.randrange(-100,101);speed=rng.choice([-2,-.5,0,.25,1,2,10]);mode=rng.choice([0,1,2,3,0xffffffff]);time=rng.uniform(-100,100);normalized=i%2
 data=w(count)+f(duration)+w(start)+f(speed)+w(mode)+f(time)+w(normalized);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,w(0xa5a5a5a5));args=list(struct.unpack('<7I',data))
 assert call('rf_vfx_texture_frame',args+[OUT])==0;got=read(x,OUT);responses.append(w(0,got));state.update(count=count,duration=args[1])
 o.mem_write(OWNER,bytes(52));o.mem_write(OWNER,w(100));o.mem_write(OWNER+40,w(start,args[3],mode));o.mem_write(STACK,w(STOP,args[5]));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x54a6e0 if normalized else 0x54a630,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=o.reg_read(UC_X86_REG_EAX)-(100 if count==1 else 101);assert got==expected,(i,args,got,expected)
for data in [w(0)+f(1)+w(0)+f(1)+w(0)+f(0)+w(0),w(2)+f(0)+w(0)+f(1)+w(0)+f(0)+w(0),w(2)+f(1)+w(0)+f(1)+w(0,0x7fc00000,1),w(2)+f(1)+w(0)+f(1)+w(0)+f(1e30)+w(1)]:
 inputs.append(data);args=list(struct.unpack('<7I',data));x.mem_write(OUT,w(0xa5a5a5a5));status=call('rf_vfx_texture_frame',args+[OUT]);assert status!=0 and read(x,OUT)==0xa5a5a5a5;responses.append(w(status,0xa5a5a5a5))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-texture-frame'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=4096,guards=4,scope='Complete54a630/54a6e0; bitmap metadata supplied at50f380, actual floor/ftol/max and wrap/clamp branches. Image index normalized from original handle result. No native XEMU.')
(root/'artifacts/vfx-texture-frame.json').write_text(json.dumps(report,indent=2));print(report)
