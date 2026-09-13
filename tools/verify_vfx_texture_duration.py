"""Original bitmap duration and retained VFX image selection."""
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



rng=random.Random(0x50f380);inputs=[];responses=[]
o.mem_write(0x17c80c4,w(FACE));o.mem_write(0x5a4554,w(1024));o.mem_write(FACE,bytes(108))
for i in range(2048):
 count=rng.randrange(1,256);rate=rng.choice([1,15,30,60,1000,2147483647,rng.randrange(1,100000)]);inputs.append(w(count,rate));x.mem_write(OUT,w(0xa5a5a5a5));assert call('rf_vfx_texture_duration',[count,rate,OUT])==0;got=bytes(x.mem_read(OUT,4));responses.append(w(0)+got)
 # Execute the actual loader's fild/fmul/fstp rate store, then complete metadata query.
 o.mem_write(STACK+0x34,w(rate));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,FACE);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x50f9cd,0x50f9da,count=20)
 o.mem_write(FACE+0x38,w(0 if count==1 else 1));o.mem_write(FACE+0x43,bytes([count]));o.mem_write(STACK,w(STOP,0,OUT));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x50f380,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 assert o.reg_read(UC_X86_REG_EAX)==count and got==bytes(o.mem_read(OUT,4)),(count,rate,got.hex(),bytes(o.mem_read(OUT,4)).hex())
 # Bind either authored texture slot and compare the selected pointer to the complete original clock.
 slot=i%2;normalized=(i//2)%2;start=rng.randrange(-10,11);speed=rng.choice([.25,1,2]);mode=i%3;time=struct.unpack('<I',f(rng.uniform(-10,100)))[0]
 if rate>1000000:start=0;time=struct.unpack('<I',f(.01))[0]
 x.mem_write(CTX,w(0,UV,FACE,1,1,0));x.mem_write(UV,w(0,0,0xffffffff));x.mem_write(FACE,bytes(64)+w(B+0x7000,count,rate,0,0));x.mem_write(OWNER,bytes(208));x.mem_write(OWNER+(27 if slot else 14)*4,w(start)+f(speed)+w(mode));x.mem_write(OUT,w(0xa5a5a5a5))
 status=call('rf_vfx_material_texture_sample',[CTX,OWNER,0,slot,time,normalized,OUT]);assert status==0,(i,count,rate,slot,normalized,status);selected=(read(x,OUT)-(B+0x7000))//20
 o.mem_write(OWNER,bytes(52));o.mem_write(OWNER+40,w(start)+f(speed)+w(mode));o.mem_write(STACK,w(STOP,time));o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x54a6e0 if normalized else 0x54a630,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 expected=o.reg_read(UC_X86_REG_EAX)-(0 if count==1 else 1);assert selected==expected,(i,selected,expected)
for count,rate in [(0,1),(256,1),(2,0),(2,0xffffffff)]:
 inputs.append(w(count,rate));x.mem_write(OUT,w(0xa5a5a5a5));status=call('rf_vfx_texture_duration',[count,rate,OUT]);assert status!=0 and read(x,OUT)==0xa5a5a5a5;responses.append(w(status,0xa5a5a5a5))
binding_guards=0
for material,slot in [(1,0),(0,2),(0,0)]:
 x.mem_write(UV,w(0xffffffff,0xffffffff,0xffffffff));x.mem_write(OUT,w(0xa5a5a5a5))
 assert call('rf_vfx_material_texture_sample',[CTX,OWNER,material,slot,0,0,OUT])!=0 and read(x,OUT)==0xa5a5a5a5;binding_guards+=1
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-texture-duration'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,nxdk_original_texture_bindings=2048,guards=4,binding_guards=binding_guards,scope='Actual50f9cd rate store and full50f380 with real handle resolver; bitmap-table inputs supplied, no hooks. NXDK primary/secondary pointer selection also compared to complete original clocks. No native XEMU.')
(root/'artifacts/vfx-texture-duration.json').write_text(json.dumps(report,indent=2));print(report)
