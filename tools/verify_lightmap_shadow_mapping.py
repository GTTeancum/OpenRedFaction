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
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_mapping_prepare\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
def stop_reject(u,a,size,ctx):u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop_reject,begin=0x4f561a,end=0x4f561a)
rng=random.Random(0x4f4637);inputs=[];responses=[];facing_count=0
for i in range(2048):
 width=2+i%63;height=2+(i//63)%63;iw=width+rng.randrange(256);ih=height+rng.randrange(256);ox=rng.randrange(iw-width+1);oy=rng.randrange(ih-height+1)
 normal=i%3;uaxis=[j for j in range(3) if j!=normal][(i//3)%2];scale=[rng.choice([-1,1])*rng.uniform(.01,2) for _ in range(2)];offset=[rng.uniform(-1,1) for _ in range(2)];plane=[rng.uniform(-2,2) for _ in range(4)];plane[normal]=rng.choice([-1,1])*rng.uniform(.1,2);origin=[rng.uniform(-100,100) for _ in range(3)]
 view=w(iw,ih,ox,oy)+f(*scale,*offset,*plane)+w(normal,uaxis);data=view+w(width,height)+f(*origin)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(OWNER+256,ox,oy,width,height));o.mem_write(OWNER+256,w(0,iw,ih));o.mem_write(OWNER+0x4c,f(*scale,*offset)+w(normal,uaxis,3-normal-uaxis));o.mem_write(OWNER+0x6c,f(*plane))
 o.mem_write(STACK,bytes(4096));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4637,0x4f4738,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4738
 positions=bytes(o.mem_read(STACK+0x8c,48))+bytes(o.mem_read(STACK+0xc8,12))
 o.mem_write(B+0x9000,f(*origin));o.mem_write(STACK+0x3c,w(B+0x9000));o.mem_write(B+0xf00c,w(OWNER));o.reg_write(UC_X86_REG_EBP,B+0xf000);o.emu_start(0x4f4b32,0x4f4b9e,count=1000000);end=o.reg_read(UC_X86_REG_EIP);assert end in (0x4f4b9e,0x4f561a);facing=int(end==0x4f4b9e);expected=positions+w(facing)
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*64);assert call(x,entry,[B,width,height,B+64,OUT])==0
 got=bytes(x.mem_read(OUT,64));assert got==expected,(i,got.hex(),expected.hex());inputs.append(data);responses.append(w(0)+expected);facing_count+=facing
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-mapping'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,w(0)),(8,w(0xffffffff)),(16,f(0)),(48,w(3)),(56,w(1)),(64,f(float('nan')))]:
 bad=bytearray(data);bad[at:at+4]=value;ww,hh=struct.unpack('<II',bad[56:64]);x.mem_write(B,bytes(bad));x.mem_write(OUT,bytes([165])*64)
 status=call(x,entry,[B,ww,hh,B+64,OUT]);assert status!=0 and bytes(x.mem_read(OUT,64))==bytes([165])*64
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-mapping'],input=bytes(bad))==w(status)+bytes([165])*64
report=dict(result='PASS',original_pc_nxdk_mappings=len(inputs),facing=facing_count,rejected=len(inputs)-facing_count,pc_nxdk_guards=6,original_sha256=sha,x87_control_word='0x027f',scope='Original4f4637..4f4738 with actual4f24a0 calls and original4f4b32 facing test. All six axes, 2..64 extents, image origins, signed scales, retained inverse U/stored inverse V. Exact64-byte corner/center/facing output. Selected light origin supplied; source selection, complete masks and native rendered verification excluded.')
(root/'artifacts/lightmap-shadow-mapping.json').write_text(json.dumps(report,indent=2));print(report)
