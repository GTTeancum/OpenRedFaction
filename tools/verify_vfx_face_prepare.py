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



rng=random.Random(0x554a80);inputs=[];responses=[];counts={'clip':0,'back':0,'visible':0}
for i in range(2048):
 vertices=f(*[rng.uniform(-10,10) for _ in range(9)]);depth=f(*[rng.uniform(-10,10) for _ in range(3)]);clips=[rng.randrange(64) for _ in range(3)] if i%3 else [0,0,0];origin=f(*[rng.uniform(-5,5) for _ in range(3)]);forward=f(*[rng.uniform(-1,1) for _ in range(3)]);mode=i%2;material=rng.randrange(-1,20);data=vertices+depth+w(*clips)+origin+forward+w(mode,material);assert len(data)==92;inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*20);assert call('rf_vfx_face_prepare',[B,OUT])==0;got=bytes(x.mem_read(OUT,20));responses.append(w(0)+got)
 o.mem_write(B,vertices);o.mem_write(FACE,bytes(0x90));o.mem_write(FACE+0x14,w(0,1,2,material));o.mem_write(FACE+0x84,w(CTX,CTX+44,CTX+88));o.mem_write(CTX,bytes(132));o.mem_write(0x1cd3840,w(0));o.mem_write(0x1c5e540,bytes(16));o.mem_write(0x17c7bcc,w(0x66));o.mem_write(0x5a4d19,bytes([mode]));o.mem_write(0x1818690,origin);o.mem_write(0x18186e0,forward)
 for j in range(3):
  at=0x1775b38+j*48;o.mem_write(at,bytes(48));o.mem_write(at+8,depth[j*4:j*4+4]);o.mem_write(at+0x18,bytes([clips[j],0,0,1]))
 o.mem_write(STACK,w(STOP,FACE,B));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(0x554a80,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 visible=read(o,0x1cd3840);expected=bytes(o.mem_read(FACE+0x60,12))+bytes(o.mem_read(0x1c5e540,4))+w(visible);assert got==expected,(i,got.hex(),expected.hex());counts['visible' if visible else 'clip' if clips[0]&clips[1]&clips[2] else 'back']+=1
for offset,value in ((48,256),(84,2)):
 bad=bytearray(data);struct.pack_into('<I',bad,offset,value);bad=bytes(bad);inputs.append(bad);x.mem_write(B,bad);x.mem_write(OUT,b'\xa5'*20);status=call('rf_vfx_face_prepare',[B,OUT]);assert status!=0 and bytes(x.mem_read(OUT,20))==b'\xa5'*20;responses.append(w(status)+b'\xa5'*20)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-face-prepare'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=2,paths=counts,scope='Complete554a80 using preprojected cached vertices, actual559f50/518460/5478f0 and depth append; no hooks. Normal/depth/visibility agree. Projection/cache lifetime, persistent list and rendering/native XEMU excluded.')
(root/'artifacts/vfx-face-prepare.json').write_text(json.dumps(report,indent=2));print(report)
