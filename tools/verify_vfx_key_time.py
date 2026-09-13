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



rng=random.Random(0x53f9b7);inputs=[];responses=[]
for i in range(2048):
 data=f(rng.uniform(-10000,10000),rng.uniform(-1000,1000));inputs.append(data)
 x.mem_write(B,data);assert call('rf_vfx_key_time',[read(x,B),read(x,B+4),OUT])==0;got=bytes(x.mem_read(OUT,4));responses.append(w(0)+got)
 o.mem_write(FACE,w(OWNER));o.mem_write(OWNER+0xa8,data[4:]);o.mem_write(STACK,w(STOP)+data[:4]);o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,FACE);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x53f060,0x53f07f,count=1000);assert o.reg_read(UC_X86_REG_EIP)==0x53f07f
 o.reg_write(UC_X86_REG_EDI,OWNER);o.emu_start(0x53f9b7,0x53f9e5,count=10000);assert o.reg_read(UC_X86_REG_EIP)==0x53f9e5
 expected=w(o.reg_read(UC_X86_REG_EBX)-o.reg_read(UC_X86_REG_EAX));assert got==expected,(i,got.hex(),expected.hex())
for data in (w(0x7fc00000,0),f(1e30,0),f(0,1e30),f(6000000,-400000)):
 inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,w(0x12345678));status=call('rf_vfx_key_time',[read(x,B),read(x,B+4),OUT]);assert status!=0 and read(x,OUT)==0x12345678;responses.append(w(status,read(x,OUT)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-key-time'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=4,scope='Original frame-to-seconds prologue and53f9b7 time conversion with actual ftol, no hooks; separate signed truncations before subtraction. No full playback/native XEMU.')
(root/'artifacts/vfx-key-time.json').write_text(json.dumps(report,indent=2));print(report)
