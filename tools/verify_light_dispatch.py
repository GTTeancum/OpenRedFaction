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



trace=[];xtrace=[];fail=0
entries=[0x4d86d0,0x517f00,0x4d8480,0x517f20]
def old_hook(cpu,address,size,ctx):
 code=entries.index(address)+1;sp=cpu.reg_read(UC_X86_REG_ESP);count=[3,2,1,0][code-1];args=list(struct.unpack('<'+'I'*count,cpu.mem_read(sp+4,count*4))) if count else []
 trace.append([code,*args,*([0]*(3-count))]);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
def new_hook(cpu,address,size,ctx):
 code=(address-(B+0xf000))//16+1;sp=cpu.reg_read(UC_X86_REG_ESP);count=[3,2,1,0][code-1];args=list(struct.unpack('<'+'I'*count,cpu.mem_read(sp+8,count*4))) if count else []
 xtrace.append([code,*args,*([0]*(3-count))]);cpu.reg_write(UC_X86_REG_EAX,0xffffffff if fail==len(xtrace) else 0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
for addr in entries:o.hook_add(UC_HOOK_CODE,old_hook,begin=addr,end=addr)
for j in range(4):x.hook_add(UC_HOOK_CODE,new_hook,begin=B+0xf000+j*16,end=B+0xf000+j*16)
x.mem_write(FACE,w(*(B+0xf000+j*16 for j in range(4)),0))
inputs=[];responses=[]
def setup(count,circular):
 o.mem_write(OWNER,w(UV if count else 0));x.mem_write(OWNER,w(UV if count else 0))
 for j in range(count):
  o.mem_write(UV+j*20,w(UV+(j+1)*20 if j+1<count else UV if circular else 0,0,20+j,30+j,40+j))
  x.mem_write(UV+j*16,w(UV+(j+1)*16 if j+1<count else UV if circular else 0,20+j,30+j,40+j))
def packet(status,events):return w(status,len(events),*[v for row in events for v in row],*([0]*(84-len(events)*4)))
for case in range(512):
 mode=[0,1,255,256,257][case%5];main=0 if case%7==0 else 11;update=case*257;count=case%6;circular=case%2;setup(count,circular);inputs.append(w(mode,main,update,count,circular,0))
 o.mem_write(0x879af8,bytes([mode&255]));o.mem_write(0xc96880,w(main));o.mem_write(0xc96884,w(OWNER));trace.clear();o.mem_write(STACK,w(STOP,7,update));o.reg_write(UC_X86_REG_ESP,STACK);o.emu_start(0x4d8660,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
 xtrace.clear();assert call('rf_visibility_light_dispatch',[mode,7,main,update,OWNER,5,FACE])==0 and xtrace==trace;responses.append(packet(0,trace))
# All21 possible service failures on a five-view dispatch, including cleanup.
full=[[1,7,11,9]]
for j in range(5):full += [[2,30+j,40+j,0],[3,7,0,0],[1,7,20+j,9],[4,0,0,0]]
for fail in range(1,22):
 setup(5,1);inputs.append(w(1,11,9,5,1,fail));expected=full[:fail]
 if fail>1 and full[fail-1][0] in (1,3):expected=expected+[[4,0,0,0]]
 xtrace.clear();assert call('rf_visibility_light_dispatch',[1,7,11,9,OWNER,5,FACE])==0xffffffff and xtrace==expected,(fail,xtrace,expected);responses.append(packet(0xffffffff,expected))
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-dispatch'],input=b''.join(inputs))==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_cases=512,pc_nxdk_failure_cases=21,scope='Actual4d8660 with supplied dirty/enter/transform/leave services; exact gates, order and arguments for empty/null-terminated/circular lists. Error cleanup verified PC/NXDK. Transform math and solid updates have separate unhooked comparisons; view-stack internals/native lifecycle excluded.')
(root/'artifacts/light-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
