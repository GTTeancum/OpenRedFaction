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



rng=random.Random(0x554b0f);inputs=[];responses=[]
for i in range(2048):
 count=i%8;material=rng.choice([-1,0,1,20,100000000,rng.randrange(-1000,1000)]);depth=f(*[rng.uniform(-1000,1000) for _ in range(3)]);records=b''.join(f(*[rng.uniform(-3,3) for _ in range(3)])+w(j) for j in range(8));data=w(count,8,material)+depth+records;inputs.append(data)
 x.mem_write(B,data);assert call('rf_vfx_face_append',[B+12,material&0xffffffff,FACE,B+24,8,B])==0;got=bytes(x.mem_read(B+24,128));responses.append(w(0,read(x,B))+got)
 o.mem_write(FACE,bytes(0x90));o.mem_write(FACE+0x20,w(material));o.mem_write(FACE+0x84,w(CTX,CTX+44,CTX+88));o.mem_write(CTX,bytes(132));o.mem_write(UV,bytes(36))
 for j in range(3):o.mem_write(UV+j*12+8,depth[j*4:j*4+4])
 o.mem_write(0x1c5e540,records);o.mem_write(0x1cd3840,w(count));o.mem_write(STACK,bytes(0x100));o.mem_write(STACK+0x10,w(UV,UV+12,UV+24));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,FACE);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(0x554b0f,0x554be7,count=10000);assert o.reg_read(UC_X86_REG_EIP)==0x554be7
 expected=bytes(o.mem_read(0x1c5e540,128));assert got==expected,(i,material);assert read(o,0x1cd3840)==count+1
for count,depth in ((8,f(0,0,0)),(9,f(0,0,0)),(0,w(0x7fc00000,0,0))):
 data=w(count,8,0)+depth+records;inputs.append(data);x.mem_write(B,data);status=call('rf_vfx_face_append',[B+12,0,FACE,B+24,8,B]);assert status!=0 and read(x,B)==count and bytes(x.mem_read(B+24,128))==records;responses.append(w(status,count)+records)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-face-append'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,guards=3,scope='Original554b0f..554be7 depth calculation and list append without hooks; preserved secondary fields, exact float key and item, varied material indices. Port capacity guards added. No preceding cull/cache, live list or native XEMU.')
(root/'artifacts/vfx-face-append.json').write_text(json.dumps(report,indent=2));print(report)
