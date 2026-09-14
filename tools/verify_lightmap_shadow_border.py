"""Replay original shadow-mask border copies against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_border\s+([0-9a-fA-F]+)',mp)[1],16)
rng=random.Random(0x4f5588);inputs=[];responses=[];small=inactive=0
for i in range(1024):
 width=2+i%63;height=2+(i//63)%63;active=int(i%5!=0)
 data=rng.randbytes(4096);o.mem_write(B,data);o.mem_write(OWNER+24,w(width,height));o.mem_write(STACK+0x23,bytes([active]));o.mem_write(STACK+0x800+24,w(B))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EBP,STACK+0x800);o.emu_start(0x4f5588,0x4f55f1,count=100000)
 assert o.reg_read(UC_X86_REG_EIP)==0x4f55f1
 expected=bytes(o.mem_read(B,4096));x.mem_write(B,data);x.mem_write(STACK,w(STOP,B,width*height,width,height,active));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(B,4096))==expected,i
 inputs.append(w(width,height,width*height,active)+data);responses.append(w(0)+expected)
 small+=int(width==2 or height==2);inactive+=int(not active)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-border'],input=b''.join(inputs))==b''.join(responses)
for width,height,size in ((1,4,4),(4,1,4),(4,4,15),(0xffffffff,2,4096)):
 x.mem_write(B,data);x.mem_write(STACK,w(STOP,B,size,width,height,1));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=100000);status=x.reg_read(UC_X86_REG_EAX)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and status!=0 and bytes(x.mem_read(B,4096))==data
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-border'],input=w(width,height,size,1)+data)==w(status)+data
report=dict(result='PASS',original_pc_nxdk_masks=len(inputs),two_pixel_extents=small,inactive=inactive,guards=4,original_sha256=sha,scope='Unhooked original4f5588..4f55f1 bytes and untouched tail; active flag supplied. Side-first and interleaved row order, including two-pixel extents. Full shadow traversal and native rendering still pending.')
(root/'artifacts/lightmap-shadow-border.json').write_text(json.dumps(report,indent=2));print(report)
