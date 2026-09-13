"""Original VFX UV copy/interpolation branches against both targets."""
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

rng=random.Random(0x54010a);inputs=[];responses=[]
for i in range(2048):
 mode=i%3;fraction=rng.choice([0.0,-0.0,1.0,.25,.75,rng.random()]);a=f(*[rng.uniform(-3,3) for _ in range(6)]);b=f(*[rng.uniform(-3,3) for _ in range(6)])
 if i<6:a=f(-0.0,0,-0.0,0,-0.0,0)
 data=a+b+f(fraction)+w(mode==1);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*24)
 status=call('rf_vfx_uv_sample',[B,B+24,struct.unpack_from('<I',data,48)[0],mode==1,OUT]);assert status==0;got=bytes(x.mem_read(OUT,24));responses.append(w(status)+got)
 o.mem_write(B,a+b);o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0x114,w(0 if mode==0 else 0x100));o.mem_write(OWNER+0xa4,w(2));o.mem_write(OWNER+0xb4,w(1));o.mem_write(OWNER+0xd4,w(PTR));o.mem_write(PTR,w(B,B if mode==2 else B+24));o.mem_write(FACE,bytes(0x98));o.mem_write(FACE+0x84,w(OUT))
 o.mem_write(STACK,bytes(0x100));o.mem_write(STACK+0x28,f(fraction));o.mem_write(STACK+0x40,w(0));o.mem_write(STACK+0x6c,w(2 if mode==2 else 1));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDX,0x80000000);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x54010a,0x5402fa,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x5402fa;assert got==bytes(o.mem_read(OUT,24)),(i,got.hex(),bytes(o.mem_read(OUT,24)).hex())
for data in [bytes(48)+f(-.1)+w(1),bytes(48)+w(0x7fc00000,1),w(0x7f800000)+bytes(44)+f(.5)+w(1)]:
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*24);status=call('rf_vfx_uv_sample',[B,B+24,struct.unpack_from('<I',data,48)[0],1,OUT]);assert status!=0 and bytes(x.mem_read(OUT,24))==b'\xa5'*24;responses.append(w(status)+bytes(x.mem_read(OUT,24)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-uv'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=3,scope='Original54010a..5402fa unhooked shared/terminal copy and animated interpolation; all six UV components match. No GPU/material transforms/native XEMU.')
(root/'artifacts/vfx-uv.json').write_text(json.dumps(report,indent=2));print(report)
