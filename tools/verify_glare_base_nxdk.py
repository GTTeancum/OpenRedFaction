"""Execute actual NXDK glare owner with only heap calls supplied."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x100000);R=B;L=B+0x10000;F=L+0x100;O=L+0x200;D=L+0x300;M=L+0x400;UID=L+0x500;HEAP=B+0x50000;S=B+0xf0000;STOP=S+0x1000
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,reginit,listinit,append,remove,lookup,calloc,free=map(sym,('rf_glare_base_open','rf_glare_base_close','rf_object_registry_init','rf_object_list_init','rf_object_list_append','rf_object_list_remove','rf_object_registry_lookup','calloc','free'))
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0];live={};allocations=0;fail=False

def hook(cpu,a,size,context):
 global allocations
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);value=0
 if a==calloc:
  allocations+=1;n=arg(0)*arg(1);assert n==528
  if not fail:assert not live;live[HEAP]=n;x.mem_write(HEAP,bytes(n));value=HEAP
 elif arg(0):n=live.pop(arg(0));x.mem_write(arg(0),b'\xdd'*n)
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,r(sp))
for a in (calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(a,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rows=json.loads((root/'artifacts/glare-base-original.json').read_text())['records']
commands=[struct.pack('<f3I',a['radius'],7 if a['parent'] else 0,17 if a['parent'] else 1,0) for a in rows]+[struct.pack('<f3I',.5,0,1,m) for m in (1,2)]
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-base'],input=b''.join(commands))
for i,command in enumerate(commands+[struct.pack('<f3I',.5,0,1,3)]):
 assert not live;allocations=0;radius,pbyte,group,mode=struct.unpack('<4I',command);fail=mode==3
 call(reginit,R);call(listinit,L);call(listinit,F);x.mem_write(O,w(0));x.mem_write(UID,w(-1))
 if mode==2:x.mem_write(R+12292,w(0))
 x.mem_write(D,w(123,radius)+struct.pack('<12f',1,2,3,1,0,0,0,1,0,0,0,1));x.mem_write(M,struct.pack('<3f',.25,.5,2))
 status=call(entry,D,R,L,UID,pbyte,group,M,528-(mode==1),O);owner=r(O);copy=bytes(528)
 if owner:
  assert call(lookup,R,r(owner+112))==owner;copy=bytearray(x.mem_read(owner,528));copy[104:112]=bytes(8);handle=r(owner+112)
  call(append,F,owner+52);assert call(close,O,R,L)==0xfffffffc and r(O)==owner and live
  call(remove,F,owner+52);assert call(close,O,R,L)==0 and call(lookup,R,handle)==0
 assert call(close,O,R,L)==0 and not live and r(O)==0
 actual=w(status,528,bool(owner),r(R+12292),r(L+8),r(UID))+copy
 if mode==3:assert actual==w(-1,528,0,1024,0,-1)+bytes(528) and allocations==1
 else:assert actual==pc[i*552:(i+1)*552],(i,actual[:24].hex(),pc[i*552:i*552+24].hex())
report=dict(result='PASS',cases=len(commands)+1,scope='Actual NXDK glare owner/physics/registry/list code versus PC bytes and lifecycle for32 original-boundary fixtures, short budget/empty registry and heap failure. Only calloc/free supplied. Family-linked close rejection, stale-handle lookup, complete retirement and no leaks. No native XEMU or live factory claim.')
(root/'artifacts/glare-base-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
