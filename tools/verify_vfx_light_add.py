"""Original light falloff profiles composed with RGB accumulation."""
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



rng=random.Random(0x4dadb4);inputs=[];responses=[];TRAMP=B+0xf000
for i in range(4096):
 profile=i%4;radius=struct.unpack('<f',f(rng.uniform(.01,100)))[0];distance=rng.choice([0,radius,rng.uniform(0,radius)]);gain=rng.uniform(0,2);color=[rng.random()*2 for _ in range(3)];accum=[rng.random()*3 for _ in range(3)];data=w(profile)+f(distance,radius,gain,*color,*accum);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12)
 words=struct.unpack('<10I',data);assert call('rf_vfx_light_add',[profile,*words[1:4],B+16,B+28,OUT])==0;got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(OUT,data[28:40]);o.mem_write(STACK,w(TRAMP,*words[1:3]));o.mem_write(STACK+4+0x28,data[16:20]);o.mem_write(STACK+4+0x20,data[20:24]);o.mem_write(STACK+4+0x24,data[24:28]);o.mem_write(TRAMP,b'\xd8\x0d'+w(B+12)+b'\x68'+w(0x4dadb4)+b'\xc3')
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBX,OUT);o.reg_write(UC_X86_REG_EBP,OUT+4);o.reg_write(UC_X86_REG_EDI,OUT+8);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start([0x4da0b0,0x4da0c0,0x4da0e0,0x4da100][profile],0x4dadd2,count=10000);assert o.reg_read(UC_X86_REG_EIP)==0x4dadd2
 assert got==bytes(o.mem_read(OUT,12)),(i,got.hex(),bytes(o.mem_read(OUT,12)).hex())
 assert call('rf_vfx_light_add',[profile,*words[1:4],B+16,B+28,B+28])==0 and bytes(x.mem_read(B+28,12))==got
for data in [w(4)+f(0,1,1,*([0]*6)),w(0)+f(0,0,1,*([0]*6)),w(3)+f(2,1,1,*([0]*6))]:
 inputs.append(data);x.mem_write(B,data);words=struct.unpack('<10I',data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_light_add',[*words[:4],B+16,B+28,OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==b'\xa5'*12;responses.append(w(status)+b'\xa5'*12)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-light-add'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=4096,in_place_checks=4096,guards=3,scope='Actual four falloff routines and original4dadb4 RGB addition, with supplied angular-gain multiply between them. No hooks. No active light owner/native XEMU.')
(root/'artifacts/vfx-light-add.json').write_text(json.dumps(report,indent=2));print(report)
