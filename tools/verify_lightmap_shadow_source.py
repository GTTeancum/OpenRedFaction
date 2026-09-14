"""Replay original shadow mapping corner/center and facing setup against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_lightmap_shadow_source_samples\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4f4738);inputs=[];responses=[];line=rounded=0
for i in range(2048):
 kind=4 if i%2 else i%7;local=[0,1,0xffffffff][i%3];positions=[rng.uniform(-1e7,1e7) for j in range(12)];radius=rng.uniform(0,1e5)
 data=w(kind)+f(*positions,radius)+w(local);o.mem_write(OWNER,bytes(144));o.mem_write(OWNER+8,w(kind));o.mem_write(OWNER+12,data[4:28]);o.mem_write(OWNER+92,data[28:52]);o.mem_write(OWNER+60,data[52:56]);o.mem_write(0x1818b84,w(local));o.mem_write(STACK,bytes(4096));o.mem_write(B+0xf010,w(OWNER))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4738,0x4f4919,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4919
 center=data[28:40] if local else data[4:16];count=struct.unpack('<I',o.mem_read(STACK+0x2c,4))[0];amount=o.mem_read(STACK+0x70,1)[0]
 origins=bytes(o.mem_read(STACK+0x11c,count*12)).ljust(24,b'\0');expected=center+bytes(o.mem_read(STACK+0x4c,24))+origins+w(count,amount);assert len(expected)==68
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*68);assert call([B,local,OUT])==0 and bytes(x.mem_read(OUT,68))==expected,i
 inputs.append(data);responses.append(w(0)+expected);line+=count==2
 if count==2:rounded+=origins[12:24]!=(data[40:52] if local else data[16:28])
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-source'],input=b''.join(inputs))==b''.join(responses)
for at,value in ((52,f(-1)),(52,f(float('inf'))),(28 if local else 4,f(float('nan')))):
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*68);status=call([B,local,OUT]);assert status!=0 and bytes(x.mem_read(OUT,68))==bytes([165])*68
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-source'],input=bytes(bad))==w(status)+bytes([165])*68
report=dict(result='PASS',original_pc_nxdk_sources=len(inputs),line_sources=line,rounded_endpoints=rounded,guards=3,original_sha256=sha,scope='Unhooked4f4738..4f4919 source-space selection, source bounds, interpolation and sample count/strength. World/cached-local records supplied, with nonzero flag values. Unused second origin is zero in port output. Transform ownership and complete source/mask dispatch remain external.')
(root/'artifacts/lightmap-shadow-source.json').write_text(json.dumps(report,indent=2));print(report)
