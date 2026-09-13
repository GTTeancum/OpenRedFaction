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
  if not fail:
   value=HEAP+(allocations-1)*1024;assert value not in live;live[value]=n;x.mem_write(value,bytes(n))
 elif arg(0):n=live.pop(arg(0));x.mem_write(arg(0),b'\xdd'*n)
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,r(sp))
for a in (calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(a,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
entry=sym('rf_glare_segment_owned_open');close=sym('rf_glare_owned_close');C=D;SERV=D+0x800;CB=B+0x80000;WRONG=F+0x800
trace=[];mode=0
x.mem_write(C,struct.pack('<2fI',.5,1,0x5c9e98));x.mem_write(SERV,w(CB+16,0));x.mem_write(M,struct.pack('<3f',.25,.5,2))
pose=struct.pack('<12f',1,0,0,0,1,0,0,0,1,1.25,-2.5,3.75)
def service(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);status=0
 assert address==CB+16
 trace.append('pose');assert arg(1)==123 and arg(2)==(7 if len(trace)==1 else 9)
 if mode==2 or (mode==3 and len(trace)==2):status=0xffffffff
 else:
  value=pose if len(trace)==1 else struct.pack('<12f',0,0,-1,0,1,0,1,0,0,5.25,1.5,-.25)
  if mode==4:value=w(0x7fc00000)+value[4:]
  x.mem_write(arg(3),value)
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,r(sp))
for a in (CB,CB+16):x.hook_add(UC_HOOK_CODE,service,begin=a,end=a)
call(reginit,R);call(listinit,L);call(listinit,F);call(listinit,WRONG);x.mem_write(UID,w(-1));owners=[];handles=[];checks=0
def create(slot,index=0,budget=528,flag=0):
 x.mem_write(slot,w(0));return call(entry,C,1,index,123,7,9,R,L,F,UID,7,17,M,budget,SERV,slot)
for i in range(3):
 trace.clear();slot=O+i*4;assert create(slot,flag=i)==0;owner=r(slot);owners.append(owner);handles.append(r(owner+112))
 assert trace==['pose','pose'] and r(F+8)==i+1 and r(L+8)==i+1 and r(R+12292)==1023-i
 assert r(owner)==0xffffffff and r(owner+4)==0xffffffff and r(owner+128)==123 and r(owner+40)==0x5c9e98 and r(owner+48)==0 and x.mem_read(owner+76,1)==bytes([1])
 assert r(owner+120)==0x6030000 and r(owner+124)==10 and r(owner+212)==0x3f800000
 assert bytes(x.mem_read(owner+152,12))==struct.pack('<3f',3.25,-.5,1.75)
 assert bytes(x.mem_read(owner+80,24))==pose[36:]+struct.pack('<3f',5.25,1.5,-.25)
 assert r(owner+72)==0 and x.mem_read(owner+9,1)==bytes(1)
 checks+=1
assert call(close,O+4,R,L,WRONG)==0xfffffffc and r(O+4)==owners[1];checks+=1
for mode,fail,budget,index in [(2,False,528,0),(3,False,528,0),(4,False,528,0),(0,True,528,0),(0,False,527,0),(0,False,528,-1),(0,False,528,1)]:
 snapshots={p:bytes(x.mem_read(p,n)) for p,n in live.items()};old_uid=r(UID);old_alloc=allocations;trace.clear()
 status=create(O+16,index=index,budget=budget)
 assert status==(0 if index else 0xfffffffc if budget==527 else 0xfffffffe if mode==4 else 0xffffffff),(mode,fail,budget,index,status)
 assert r(O+16)==0 and r(F+8)==3 and r(L+8)==3 and r(UID)==old_uid
 assert snapshots=={p:bytes(x.mem_read(p,n)) for p,n in live.items()}
 assert trace==([] if index else ['pose'] if mode in (2,4) else ['pose','pose'])
 assert allocations==old_alloc+(fail and index==0);checks+=1
mode=0;fail=False
for i in (1,0,2):
 assert call(close,O+i*4,R,L,F)==0 and r(O+i*4)==0 and call(lookup,R,handles[i])==0;checks+=1
assert not live and r(F+8)==r(L+8)==0 and r(R+12292)==1024
assert call(close,O,R,L,F)==0;checks+=1
x.mem_write(R+12292,w(0));trace.clear();assert create(O)==0 and r(O)==0 and trace==['pose','pose'] and not live;checks+=1
report=dict(result='PASS',checks=checks,scope='Actual compiled NXDK full two-tag owned factory/constructor/base/registry/list/physics code; only pose and heap supplied. Three simultaneous owners; state, ordered insertion, wrong-family close, first/second pose, invalid pose, heap and short-budget failures, invalid classes, empty registry, middle/head/tail retirement, repeat close and no leaks. Component original equivalence checked separately; no native XEMU or campaign class binding.')
(root/'artifacts/glare-segment-owned-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
