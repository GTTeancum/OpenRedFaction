"""Compiled NXDK VFX texture ownership with missing archive resources."""
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
o=machine(exe);x=machine(root/'build/xbox/main.exe');stream=dict(data=b'',at=0)
lookup_names=[]
def file_service(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);pop=0;result=0
 if a==0x52cf60:
  target=read(u,sp+4);amount=read(u,sp+8);assert read(u,sp+12)==read(u,sp+16)==0
  chunk=stream['data'][stream['at']:stream['at']+amount];assert len(chunk)==amount,(hex(a),stream['at'],amount,len(stream['data']))
  u.mem_write(target,chunk);stream['at']+=amount;pop=16
 elif a==0x50f6a0:
  pointer=read(u,sp+4);name=bytes(u.mem_read(pointer,33)).split(b'\0')[0];lookup_names.append(name);result=0xffffffff
 else:assert a==0x524530
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
for a in (0x52cf60,0x524530,0x50f6a0):o.hook_add(UC_HOOK_CODE,file_service,begin=a,end=a)
PARAM=B+0x7000;COUNTS=B+0x8000;BLEND=B+0x9000;COLOR=B+0xa000;ALPHA=B+0xb000;SAMPLE=B+0xc000
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)

HEAP=0x31000000;x.mem_map(HEAP,0x100000);live={};fail=False

def alloc(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);arg=read(u,sp+4);result=0
 if a==sym('calloc'):
  n=arg*read(u,sp+8);assert n<=0x100000 and not live
  if not fail:live[HEAP]=n;u.mem_write(HEAP,bytes(n));result=HEAP
 else:
  if arg:assert arg in live;del live[arg]
 u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,read(u,sp))
for name in ('calloc','free'):x.hook_add(UC_HOOK_CODE,alloc,begin=sym(name),end=sym(name))
def run(views,budget,failed=False):
 global fail
 fail=failed;assert not live;x.mem_write(B,b''.join(views) or b'\0');x.mem_write(OUT,bytes(24))
 status=call('rf_vfx_material_textures_open',[OUT,B,len(views),0,0,budget]);signed=struct.unpack('<i',w(status))[0]
 storage,bindings,textures,count,n,resident=struct.unpack('<6I',x.mem_read(OUT,24));lines=[f'{signed} {count} {n} {resident}']
 if status:assert bytes(x.mem_read(OUT,24))==bytes(24) and not live
 else:
  for i in range(count):lines.append('B '+' '.join(map(str,struct.unpack('<3I',x.mem_read(bindings+i*12,12)))))
  for i in range(n):
   name=bytes(x.mem_read(textures+i*84,64)).split(b'\0')[0].decode();assert bytes(x.mem_read(textures+i*84+64,20))==bytes(20);lines.append(f'T {name} 0 0 0 0')
 call('rf_vfx_material_textures_close',[OUT]);call('rf_vfx_material_textures_close',[OUT]);assert not live and bytes(x.mem_read(OUT,24))==bytes(24)
 return lines
raw=(root/'artifacts/vfx-texture-views.bin').read_bytes();n=struct.unpack_from('<I',raw)[0];views=[raw[4+i*208:212+i*208] for i in range(n)];cases=[(views,24+n*264),([],24),(views,24+n*264-1)]
bad=bytearray(views[0]);bad[204:208]=w(8);cases.append(([bytes(bad)],1000000))
bad=bytearray(views[0]);bad[20:53]=b'x'*33;bad[204:208]=w(1);cases.append(([bytes(bad)],1000000))
for rows,budget in cases:
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_material_probe.exe'),'--vfx-textures',str(budget),str(root/'Installed_Game/tables.vpp')],input=w(len(rows))+b''.join(rows)).decode().splitlines();assert run(rows,budget)==pc
assert run(views,1000000,True)==['-1 0 0 0']
report=dict(result='PASS',pc_nxdk_cases=len(cases),allocation_failure_cases=1,scope='Compiled real material/animation ownership code with zero archives; only calloc/free supplied. Missing slots, malformed views, budget, allocation failure and repeated cleanup. Actual pixel archive decoding separately PC-verified, no native XEMU.')
(root/'artifacts/vfx-textures-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
