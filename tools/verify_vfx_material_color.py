"""Original VFX edge brightness minimum and material tint."""
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



rng=random.Random(0x554303);inputs=[];responses=[]
for i in range(2048):
 kind=i%3;tint=bytes(rng.randrange(256) for _ in range(3));lit=bytes(rng.randrange(256) for _ in range(3));brightness=f(rng.random());flags=0x10 if i%4==0 else 0;data=w(kind)+tint+lit+brightness+w(flags);inputs.append(data)
 x.mem_write(OWNER,bytes(208));x.mem_write(OWNER,w(kind));x.mem_write(OWNER+9,tint);x.mem_write(B,lit);x.mem_write(OUT,b'\xa5'*3);bits=struct.unpack('<I',brightness)[0]
 assert call('rf_vfx_material_color',[OWNER,B,bits,flags,OUT])==0;got=bytes(x.mem_read(OUT,3));responses.append(w(0)+got)
 o.mem_write(OWNER,bytes(200));o.mem_write(OWNER,w(kind));o.mem_write(OWNER+9,tint);o.mem_write(OWNER+0x78,w(15));o.mem_write(OWNER+0xb8,w(1,B));o.mem_write(B,brightness);o.mem_write(FACE+0x19,lit);o.mem_write(STACK+0x10,w(OWNER));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_EAX,0);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_EDI,FACE+0x19);o.reg_write(UC_X86_REG_EBX,FACE+0x1a);o.reg_write(UC_X86_REG_EBP,FACE+0x1b);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x5542ad if flags else 0x554303,0x5543a5,count=10000);assert o.reg_read(UC_X86_REG_EIP)==0x5543a5
 assert got==bytes(o.mem_read(FACE+0x19,3)),(i,got.hex(),bytes(o.mem_read(FACE+0x19,3)).hex())
 x.mem_write(B,lit);assert call('rf_vfx_material_color',[OWNER,B,bits,flags,B])==0 and bytes(x.mem_read(B,3))==got
for brightness in (f(-1),f(2),w(0x7fc00000)):
 inputs.append(w(2)+bytes(6)+brightness+w(0));x.mem_write(OUT,b'\xa5'*3);status=call('rf_vfx_material_color',[OWNER,B,struct.unpack('<I',brightness)[0],0,OUT]);assert status!=0 and bytes(x.mem_read(OUT,3))==b'\xa5'*3;responses.append(w(status)+b'\xa5'*3)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-material-color'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,in_place_checks=2048,guards=3,scope='Original553ee0 edge-color blocks with real brightness sampler, rounded byte conversion and tint ftol. Supplied light RGB; no scene light query/specular/glare/native XEMU.')
(root/'artifacts/vfx-material-color.json').write_text(json.dumps(report,indent=2));print(report)
