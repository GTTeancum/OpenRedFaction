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


# Original quaternion conversion and point rotation/addition, without hooks.
def original_call(address,this,args):
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,this);o.reg_write(UC_X86_REG_FPCW,0x37f)
 o.emu_start(address,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
rng=random.Random(0x5194c0);inputs=[];responses=[]
for i in range(2048):
 values=[rng.uniform(-4,4) for _ in range(13)]
 if i%8==0:values[3:7]=[0,0,0,1]
 if i%8==1:values[3:7]=[0,0,0,0]
 if i%8==2:values[7:10]=[0,0,0]
 data=f(*values);values=struct.unpack('<13f',data);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_transform_point',[B,B+40,OUT]);assert status==0;got=bytes(x.mem_read(OUT,12));responses.append(w(0)+got)
 o.mem_write(B,data);original_call(0x5194c0,B+12,[UV])
 # Scale products are individually stored as float by53f060 before4facb0.
 o.mem_write(OUT,f(*[values[10+j]*values[7+j] for j in range(3)]))
 original_call(0x4facb0,OUT,[UV]);original_call(0x40a350,OUT,[B])
 expected=bytes(o.mem_read(OUT,12));assert got==expected,(i,got.hex(),expected.hex())
 # In-place use must preserve all source coordinates until the last store.
 x.mem_write(B,data);assert call('rf_vfx_transform_point',[B,B+40,B+40])==0
 assert bytes(x.mem_read(B+40,12))==got
for index in (0,3,7,10):
 data=bytearray(f(*([0]*6+[1]*7)));struct.pack_into('<I',data,index*4,0x7fc00000);data=bytes(data);inputs.append(data)
 x.mem_write(B,data);x.mem_write(OUT,b'\xa5'*12);status=call('rf_vfx_transform_point',[B,B+40,OUT]);assert status!=0 and bytes(x.mem_read(OUT,12))==b'\xa5'*12;responses.append(w(status)+bytes(x.mem_read(OUT,12)))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-transform'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=2048,in_place_cases=2048,guards=4,scope='Actual5194c0 quaternion matrix,4facb0 rotation and40a350 translation; independent binary32 scale products. Includes nonunit and zero quaternion. No full53f060 branch, key composition, renderer or native XEMU claim.')
(root/'artifacts/vfx-transform.json').write_text(json.dumps(report,indent=2));print(report)
