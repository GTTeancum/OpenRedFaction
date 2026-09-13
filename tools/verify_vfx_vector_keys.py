"""Cubic translation/scale keys against original full evaluators."""
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

rng=random.Random(0x569f70);inputs=[];responses=[]
for i in range(2048):
 count=i%9;time=rng.randrange(-250,250);at=-200;data=b''
 for j in range(count):at+=rng.randrange(0,31);data+=w(at)+f(*[rng.uniform(-10,10) for _ in range(9)])
 inputs.append(w(count,time)+data);x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_vector_key_sample',[B,len(data),count,time&0xffffffff,OUT]);assert status==0;got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 for entry in (0x569f70,0x56a3f0):
  o.mem_write(B,data or b'\0');o.mem_write(OWNER,bytes(20));o.mem_write(OWNER+(0 if entry==0x569f70 else 4),struct.pack('<H',count));o.mem_write(OWNER+(8 if entry==0x569f70 else 16),w(B));o.mem_write(STACK,w(STOP,OUT,time));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f)
  o.emu_start(entry,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP;assert got==bytes(o.mem_read(OUT,12)),(i,hex(entry),got.hex(),bytes(o.mem_read(OUT,12)).hex())
invalid=[(2,0,w(2)+bytes(36)+w(1)+bytes(36)),(1,0,w(0,0x7fc00000)+bytes(32)),(2,0,w(-2147483648)+bytes(36)+w(2147483647)+bytes(36))]
for count,time,data in invalid:
 inputs.append(w(count,time)+data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_vector_key_sample',[B,len(data),count,time,OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==b'\xa5'*12;responses.append(w(status)+bytes(x.mem_read(OUT,12)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-vector-keys'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',cases_per_original_evaluator=2048,original_evaluators=['569f70 translation','56a3f0 scale'],guards=len(invalid),scope='Complete unhooked original evaluators and actual cubic/vector helpers versus PC/NXDK, including empty/endpoint/duplicate-time cases. No quaternion/key-time conversion/parent/rendering/native XEMU.')
(root/'artifacts/vfx-vector-keys.json').write_text(json.dumps(report,indent=2));print(report)
