"""Original VFX vertex-animation branches against shared morph sampler."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');stream=dict(data=b'',at=0)
lookup_names=[]
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);pop=0;result=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount,(hex(a),stream['at'],amount,len(stream['data']))
  u.mem_write(target,chunk);stream['at']+=amount;pop=16
 elif a==0x50f6a0:
  pointer=read(u,sp+4);name=bytes(u.mem_read(pointer,33)).split(b'\0')[0];lookup_names.append(name);result=0xffffffff
 else:assert a==0x524530
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524530,0x50f6a0):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
PARAM=B+0x7000;COUNTS=B+0x8000;BLEND=B+0x9000;COLOR=B+0xa000;ALPHA=B+0xb000;SAMPLE=B+0xc000
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

FRAMES=B+0x7000;rng=random.Random(0x53f7c3);inputs=[];responses=[]
for i in range(2048):
 mode=i%2;fraction=rng.choice([0,.25,.5,.75,1,rng.random()]);a=f(*[rng.uniform(-3,3) for _ in range(8)]);b=f(*[rng.uniform(-3,3) for _ in range(8)]);av=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);bv=struct.pack('<3h',*[rng.randrange(-32768,32768) for _ in range(3)]);data=a+av+b+bv+f(fraction)+w(mode);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);status=call('rf_vfx_morph_read',[B,B+32,B+38,B+70,struct.unpack_from('<I',data,76)[0],mode,OUT]);assert status==0;got=bytes(x.mem_read(OUT,32));responses.append(w(0)+got)
 o.mem_write(B,av+(bv if mode else av));o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x114,w(4));o.mem_write(OWNER+0xa4,w(2));o.mem_write(OWNER+0xb0,w(1));o.mem_write(OWNER+0xcc,w(FRAMES));o.mem_write(FRAMES,w(0x80000000,B)+a+w(0x80000000,B+6)+(b if mode else a));o.mem_write(FACE,bytes(0x98));o.mem_write(FACE,w(OWNER));o.mem_write(FACE+0x80,w(OUT))
 o.mem_write(STACK,bytes(0x100));o.mem_write(STACK+0x28,f(fraction));o.mem_write(STACK+0x40,w(0));o.mem_write(STACK+0x6c,w(1));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,2);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDI,OWNER);o.reg_write(UC_X86_REG_EAX,4);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53f7c3 if mode else 0x53f135,0x54010a,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x54010a
 expected=bytes(o.mem_read(FACE+4,20))+bytes(o.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex())
for data in [bytes(76)+f(-.1)+w(1),w(0x7fc00000)+bytes(72)+f(.5)+w(1)]:
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);status=call('rf_vfx_morph_read',[B,B+32,B+38,B+70,struct.unpack_from('<I',data,76)[0],1,OUT]);assert status!=0 and bytes(x.mem_read(OUT,32))==b'\xa5'*32;responses.append(w(status)+bytes(x.mem_read(OUT,32)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-morph'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=2,scope='Original vertex-animation interpolation and terminal branches with actual vector/dequantization helpers, no service hooks. Center, extra values and one expanded vertex match; no transform-key/parent/rendering/native XEMU.')
(root/'artifacts/vfx-morph.json').write_text(json.dumps(report,indent=2));print(report)
