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



rng=random.Random(0x4d8ed0);inputs=[];responses=[];P=0xc4e7d8
for i in range(2048):
 kind=2+i%3;data=w(kind,i%4,i%512,i%512)+f(*[rng.uniform(-10,10) for _ in range(9)],*[rng.random()*2 for _ in range(3)],rng.choice([0,.01,.1,.100001,1,10]),rng.random()*3,rng.uniform(0,1),rng.uniform(1.1,6),rng.random());inputs.append(data);x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*80);assert call('rf_vfx_light_create',[B,OUT])==0;got=bytes(x.mem_read(OUT,80));responses.append(w(0)+got)
 o.mem_write(B,data);o.mem_write(P,bytes(268));o.mem_write(0xc96768,w(0xc96768,0xc96768));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8));o.mem_write(0xc96880,w(0));o.mem_write(0x879af8,bytes([i%2]));o.mem_write(0xc96874,w(100));o.mem_write(0xc96878,w(0))
 color=[read(o,B+a) for a in (68,52,56,60)];tail=[read(o,B+8),0,read(o,B+4)]
 if kind==2:address=0x4d8ed0;args=[B+16,read(o,B+64),*color,*tail]
 elif kind==4:address=0x4d9050;args=[B+16,B+28,read(o,B+64),*color,*tail]
 else:address=0x4d8f80;args=[B+16,B+40,read(o,B+72),read(o,B+76),read(o,B+64),*color,read(o,B+8),0,read(o,B+80),read(o,B+4),read(o,B+12)]
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP and o.reg_read(UC_X86_REG_EAX)==0
 expected=bytearray(80);expected[:4]=o.mem_read(P+8,4);expected[4:8]=o.mem_read(P+0x54,4);expected[8:44]=o.mem_read(P+12,36);expected[44:56]=o.mem_read(P+0x40,12);expected[56:60]=o.mem_read(P+0x3c,4);expected[60:64]=o.mem_read(P+0x38,4);expected[64:72]=o.mem_read(P+0x84,8);expected[72]=o.mem_read(P+0x4e,1)[0];expected[76:78]=o.mem_read(P+0x4c,2)
 assert got==expected,(i,kind,got.hex(),expected.hex())
 assert read(o,0xc96874)==101 and read(o,0xc96878)==1
for offset,value in [(0,w(1)),(64,f(-1)),(68,f(float('nan')))]:
 bad=bytearray(inputs[0]);bad[offset:offset+4]=value;inputs.append(bytes(bad));x.mem_write(B,bytes(bad));x.mem_write(OUT,b'\xa5'*80);status=call('rf_vfx_light_create',[B,OUT]);assert status!=0 and bytes(x.mem_read(OUT,80))==b'\xa5'*80;responses.append(w(status)+b'\xa5'*80)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-light-create'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=3,scope='Complete original point/cone/segment constructors4d8ed0/4d8f80/4d9050 with real pool-slot insertion and cone conversion4d9520. Fresh zeroed original slot, no hooks, empty scene visibility owner. Compare all80 represented candidate bytes; registration/refcounts/visibility/radius-squared metadata are not ported by this initializer. Native creation and scene ownership remain.')
(root/'artifacts/vfx-light-create.json').write_text(json.dumps(report,indent=2));print(report)
