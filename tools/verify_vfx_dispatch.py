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



FRAMES=B+0x7000;TRANS=B+0x8000;KEYS=B+0x9000;rng=random.Random(0x53f060);inputs=[];responses=[]
for i in range(2048):
 mode=i%3;flags=(4 if mode==0 else 0)|rng.choice([0,1,2,0x800,0x801]);packed=60|(2 if mode==2 else 0);time=rng.choice([0,.25,.75,1,1.25,1.75]);fa=f(*[rng.uniform(-3,3) for _ in range(8)]);fb=f(*[rng.uniform(-3,3) for _ in range(8)]);va=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);vb=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);a=f(*[rng.uniform(-2,2) for _ in range(10)]);b=f(*[rng.uniform(-2,2) for _ in range(10)]);base=f(*[rng.uniform(-2,2) for _ in range(10)]);payload=fa+va+fb+vb
 header=bytearray(300);struct.pack_into('<I',header,132,1);struct.pack_into('<II',header,144,packed,2);struct.pack_into('<I',header,184,flags);struct.pack_into('<I',header,204,packed);header[212:252]=base;struct.pack_into('<I',header,292,76)
 frames=bytearray(224);frames[:32]=fa;frames[112:144]=fb;struct.pack_into('<II',frames,32,32,6);struct.pack_into('<II',frames,144,70,6);frames[48:88]=a;frames[160:200]=b;struct.pack_into('<I',frames,104,17);struct.pack_into('<I',frames,216,17)
 data=bytes(header+frames)+payload+f(time);inputs.append(data)
 x.mem_write(OWNER,bytes(header)+w(FRAMES,B));x.mem_write(FRAMES,bytes(frames));x.mem_write(B,payload);x.mem_write(OUT,b'\xa5'*32);assert call('rf_vfx_mesh_sample',[OWNER,struct.unpack('<I',f(time))[0],0,OUT])==0;got=bytes(x.mem_read(OUT,32));responses.append(w(0)+got)
 o.mem_write(B,payload);o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x88,w(packed));o.mem_write(OWNER+0x114,w(flags));o.mem_write(OWNER+0xa4,w(2));o.mem_write(OWNER+0xb0,w(1));o.mem_write(OWNER+0xcc,w(FRAMES));o.mem_write(OWNER+0xdc,w(TRANS));o.mem_write(OWNER+0xe0,w(KEYS));o.mem_write(OWNER+0xe4,base);o.mem_write(KEYS,bytes(20));o.mem_write(FRAMES,w(0x80000000,B+32)+fa+w(0x80000000,B+70)+fb);o.mem_write(TRANS,a+b);o.mem_write(FACE,bytes(0x98));o.mem_write(FACE,w(OWNER));o.mem_write(FACE+0x80,w(OUT));o.mem_write(STACK,w(STOP)+f(time));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,FACE);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53f060,0x54010a,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x54010a
 expected=bytes(o.mem_read(FACE+4,20))+bytes(o.mem_read(OUT,12));assert got==expected,(i,mode,time,flags,got.hex(),expected.hex())
for time in (-10,100,float('nan')):
 x.mem_write(OUT,b'\xa5'*32);status=call('rf_vfx_mesh_sample',[OWNER,struct.unpack('<I',f(time))[0],0,OUT]);assert status!=0 and bytes(x.mem_read(OUT,32))==b'\xa5'*32
 inputs.append(data[:-4]+f(time));responses.append(w(status)+b'\xa5'*32)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-mesh-sample'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,inactive_invalid_guards=3,scope='Original53f060 entry through geometry exit54010a; morph, non-keyed, empty-key legacy fallbacks, interpolation and terminal. No hooks; nonempty key evaluators separately verified. UV/parent/rendering/native XEMU remain.')
(root/'artifacts/vfx-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
