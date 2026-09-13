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



rng=random.Random(0x5402fa);inputs=[];responses=[]
for i in range(2048):
 data=f(*[rng.uniform(-100,100) for _ in range(20)]);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);assert call('rf_vfx_parent_sample',[B,B+32,OUT])==0;got=bytes(x.mem_read(OUT,32));responses.append(w(0)+got)
 o.mem_write(OWNER,bytes(0x124));o.mem_write(OWNER+0xb0,w(1));o.mem_write(FACE,bytes(0x98));o.mem_write(FACE,w(OWNER)+data[:20]);o.mem_write(FACE+0x1c,w(0x40000000));o.mem_write(FACE+0x20,data[68:80]+data[32:68]);o.mem_write(FACE+0x80,w(OUT));o.mem_write(OUT,data[20:32]);o.mem_write(STACK,bytes(0x200));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,FACE);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_EBP,FACE+4);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x5402fa,0x540358,count=10000);assert o.reg_read(UC_X86_REG_EIP)==0x540358
 expected=bytes(o.mem_read(FACE+4,20))+bytes(o.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex())
 x.mem_write(B,data);assert call('rf_vfx_parent_sample',[B,B+32,B])==0;assert bytes(x.mem_read(B,32))==got
for index in (0,3,5,8,19):
 data=bytearray(bytes(80));struct.pack_into('<I',data,index*4,0x7fc00000);data=bytes(data);inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*32);status=call('rf_vfx_parent_sample',[B,B+32,OUT]);assert status!=0 and bytes(x.mem_read(OUT,32))==b'\xa5'*32;responses.append(w(status)+bytes(x.mem_read(OUT,32)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-parent'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,in_place_cases=2048,guards=5,scope='Complete direct-parent branch5402fa..540358 with actual vector helpers, no hooks. Center/vertex transform and unchanged extras. Tag-based parent lookup, rendering and native XEMU remain.')
(root/'artifacts/vfx-parent.json').write_text(json.dumps(report,indent=2));print(report)
