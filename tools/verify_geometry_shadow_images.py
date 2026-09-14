"""Replay original4f25a0 shadow polygon area against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_geometry_material_shadow_images\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call():
 x.mem_write(STACK,w(STOP,OWNER,1,B+0x1000,OUT,8));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x551ec0);missing=0
for case in range(256):
 slots=[rng.randrange(8) for _ in range(11)];statuses=[0 if rng.randrange(3) else -3 for _ in range(8)]
 x.mem_write(B+0x1000,bytes(68));x.mem_write(B+0x100c,w(8));x.mem_write(B+0x2000,w(0,3,11));x.mem_write(B+0x2100,w(*slots));x.mem_write(B+0x3000,b''.join(w(0,0,0,i,0,status&0xffffffff,0) for i,status in enumerate(statuses)))
 x.mem_write(OWNER,w(B+0x3000,8,0,0,0,B+0x2000,B+0x2100,2,0,0));x.mem_write(OUT,bytes([165])*32)
 assert call()==0
 expected=[0 if statuses[i]==-3 else B+0x3000+i*28 for i in slots[3:]];assert bytes(x.mem_read(OUT,32))==w(*expected);missing+=expected.count(0)
for kind in range(3):
 x.mem_write(OUT,bytes([165])*32)
 if kind==0:x.mem_write(B+0x2008,w(10))
 if kind==1:x.mem_write(B+0x2008,w(11));x.mem_write(B+0x210c,w(8))
 if kind==2:x.mem_write(B+0x210c,w(slots[3]));x.mem_write(B+0x3000+slots[-1]*28+20,w(0xffffffff))
 assert call()!=0 and bytes(x.mem_read(OUT,32))==bytes([165])*32
report=dict(result='PASS',nxdk_tables=256,borrowed_entries=2048,missing_null_entries=missing,preserved_output_guards=3,scope='Compiled NXDK borrowed pointers, repeated material slots, nonzero geometry offset, absent images and invalid mapping/load-status guards. PC campaign bindings checked by material_probe. Does not implement animation or original bitmap ownership.')
(root/'artifacts/geometry-shadow-images.json').write_text(json.dumps(report,indent=2));print(report)
