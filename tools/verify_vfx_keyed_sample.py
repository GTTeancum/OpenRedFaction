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


# Original quaternion conversion and point rotation/addition, without hooks.
def original_call(address,this,args):
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,this);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(address,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP

FRAMES=B+0x7000;rng=random.Random(0x53fb11);inputs=[];responses=[]
for i in range(2048):
 flags=rng.choice([0,1,0x800,0x801]);frame=f(*[rng.uniform(-3,3) for _ in range(8)]);vertex=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);base=f(*[rng.uniform(-2,2) for _ in range(10)]);key=f(*[rng.uniform(-2,2) for _ in range(10)]);data=frame+vertex+base+key+w(flags);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);assert call('rf_vfx_keyed_sample',[B,B+32,B+38,B+78,flags,OUT])==0;got=bytes(x.mem_read(OUT,32));responses.append(w(0)+got)
 o.mem_write(B,data);original_call(0x5194c0,B+50,[UV]);original_call(0x5194c0,B+90,[PTR]);bm=bytes(o.mem_read(UV,36));km=bytes(o.mem_read(PTR,36))
 o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x114,w(flags));o.mem_write(OWNER+0xb0,w(1));o.mem_write(OWNER+0xcc,w(FRAMES));o.mem_write(FRAMES,w(0x80000000,B+32)+frame);o.mem_write(FACE,bytes(0x98));o.mem_write(FACE,w(OWNER));o.mem_write(FACE+0x80,w(OUT))
 o.mem_write(STACK,bytes(0x200));o.mem_write(STACK+0x18,base[28:40]);o.mem_write(STACK+0x2c,key[28:40]);o.mem_write(STACK+0x70,base[:12]);o.mem_write(STACK+0x54,key[:12]);o.mem_write(STACK+0x8c,bm);o.mem_write(STACK+0xb0,km)
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDI,0);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53fb11,0x54010a,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x54010a
 expected=bytes(o.mem_read(FACE+4,20))+bytes(o.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-keyed-sample'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,scope='Resolved keyed geometry53fb11..54010a with actual5194c0 matrices, dequantization and vector helpers, no hooks. Base then key pose, center/vertex/extra comparison. Key selection/time and parent/rendering/native XEMU remain separate.')
(root/'artifacts/vfx-keyed-sample.json').write_text(json.dumps(report,indent=2));print(report)
