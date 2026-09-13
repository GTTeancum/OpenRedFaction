"""Composed VFX rotation key evaluation against original56a250."""
import math
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

rng=random.Random(0x56a250);inputs=[];responses=[]
for i in range(2048):
 count=0 if i%9==0 else i%7+2;time=rng.randrange(-200,250);at=-150;data=b'';converted=b''
 for j in range(count):
  at+=rng.randrange(1,40);q=[rng.uniform(-1,1) for _ in range(4)];length=math.sqrt(sum(v*v for v in q));row=w(at)+f(*[v/length for v in q])+f(0,0,0,rng.randrange(128),rng.randrange(128));data+=row
  vals=struct.unpack_from('<9f',row,4);converted+=w(at)+struct.pack('<4h',*[int(v*16383) for v in vals[:4]])+bytes(int(v)&255 for v in vals[4:])+bytes(3)
 inputs.append(w(count,time)+data);x.mem_write(B,data or b'\0');x.mem_write(OUT,b'\xa5'*16);status=call('rf_vfx_rotation_key_sample',[B,len(data),count,time&0xffffffff,OUT]);assert status==0,(i,status);got=bytes(x.mem_read(OUT,16));responses.append(w(0)+got)
 o.mem_write(B,converted or b'\0');o.mem_write(OWNER,bytes(20));o.mem_write(OWNER+2,struct.pack('<H',count));o.mem_write(OWNER+12,w(B));o.mem_write(STACK,w(STOP,OUT,time,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x56a250,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP;assert got==bytes(o.mem_read(OUT,16)),(i,time,got.hex(),bytes(o.mem_read(OUT,16)).hex())
# Singleton deliberately avoids the original out-of-bounds adjacent-key read.
data=w(10)+f(0,0,0,1,0,0,0,0,0)
for time in (-1,10,20):
 inputs.append(w(1,time)+data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*16);assert call('rf_vfx_rotation_key_sample',[B,len(data),1,time&0xffffffff,OUT])==0;got=bytes(x.mem_read(OUT,16));assert got==f(0,0,0,1);responses.append(w(0)+got)
invalid=[(1,0,w(0)+f(0,0,0,1,0,0,0,128,0)),(2,0,w(2)+f(0,0,0,1,0,0,0,0,0)+w(1)+f(0,0,0,1,0,0,0,0,0))]
for count,time,data in invalid:
 inputs.append(w(count,time)+data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*16);status=call('rf_vfx_rotation_key_sample',[B,len(data),count,time,OUT]);assert status!=0 and bytes(x.mem_read(OUT,16))==b'\xa5'*16;responses.append(w(status)+bytes(x.mem_read(OUT,16)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-rotation-keys'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,singleton_checks=3,guards=len(invalid),scope='Complete unhooked56a250 with real easing/interpolation/expansion; independently packed serialized inputs. Singleton safely decodes directly; negative easing rejected. No native XEMU/composed pose rendering.')
(root/'artifacts/vfx-rotation-keys.json').write_text(json.dumps(report,indent=2));print(report)
