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



SAMPLE=B+0xc000;FRAMES=B+0x7000;TRANS=B+0x8000;rng=random.Random(0x53fd10);inputs=[];responses=[]
for i in range(2048):
 mode=i%2;t=rng.choice([0,.25,.5,.75,1,rng.random()]);flags=rng.choice([0,1,0x800,0x801]);frame=f(*[rng.uniform(-3,3) for _ in range(8)]);av=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);a=f(*[rng.uniform(-2,2) for _ in range(10)]);b=f(*[rng.uniform(-2,2) for _ in range(10)]);data=frame+av+a+b+f(t)+w(mode,flags);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);status=call('rf_vfx_transform_sample',[B,B+32,B+38,B+78,struct.unpack_from('<I',data,118)[0],mode,flags,OUT]);assert status==0;got=bytes(x.mem_read(OUT,32));responses.append(w(0)+got)
 o.mem_write(B,av);o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x114,w(flags));o.mem_write(OWNER+0xa4,w(2));o.mem_write(OWNER+0xb0,w(1));o.mem_write(OWNER+0xcc,w(FRAMES));o.mem_write(OWNER+0xdc,w(TRANS));o.mem_write(FRAMES,w(0x80000000,B)+frame);o.mem_write(TRANS,a+(b if mode else a));o.mem_write(FACE,bytes(0x98));o.mem_write(FACE,w(OWNER));o.mem_write(FACE+0x80,w(OUT))
 o.mem_write(STACK,bytes(0x200));o.mem_write(STACK+0x28,f(t));o.mem_write(STACK+0x40,w(0));o.mem_write(STACK+0x6c,w(1));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDI,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53fd10 if mode else 0x53f5c4,0x54010a,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x54010a
 expected=bytes(o.mem_read(FACE+4,20))+bytes(o.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex())
 # Owned-view addressing fixture: shared frame0 vertices, per-frame transforms.
 x.mem_write(OWNER,bytes(308));x.mem_write(OWNER+132,w(1));x.mem_write(OWNER+148,w(2));x.mem_write(OWNER+184,w(flags));x.mem_write(OWNER+292,w(len(data)));x.mem_write(OWNER+300,w(FRAMES,B))
 x.mem_write(FRAMES,bytes(224));x.mem_write(FRAMES,frame);x.mem_write(FRAMES+32,w(32,6));x.mem_write(FRAMES+48,a);x.mem_write(FRAMES+104,w(17));x.mem_write(FRAMES+112+48,b);x.mem_write(FRAMES+112+104,w(16))
 x.mem_write(PTR,f(0,t)+w(0,1 if mode else 0,1));x.mem_write(SAMPLE,b'\xa5'*32)
 assert call('rf_vfx_mesh_transform',[OWNER,PTR,0,SAMPLE])==0
 assert bytes(x.mem_read(SAMPLE,32))==got

pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-transform-sample'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,mesh_accessor_fixtures=2048,scope='Complete non-keyed transform interior and terminal branches53fd10/53f5c4 through54010a, actual dequantization and vector helpers without hooks. Center, extras and vertex; no parent/rendering/native XEMU.')
(root/'artifacts/vfx-transform-sample.json').write_text(json.dumps(report,indent=2));print(report)
