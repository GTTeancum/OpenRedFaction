"""Audit original type4 generic allocation around supplied model services."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);u.mem_map(0,4096)
b=0x30000000;u.mem_map(b,0x20000);params=b;source=b+0x2000;stack=b+0xe000;stop=b+0xf000;pool=0x708748;actor=b+0x4000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def put(a,v):u.mem_write(a,w(v))
allocations=[];trace=[];fail_object=False;fail_model=False;freed=[]
model=b+0x18000;parent=b+0x19000

def hook(cpu,address,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+4*i);result=0;pop=0
 trace.append(address)
 if address==0x573619:
  n=arg(0);assert n in (748,384),n
  result=actor if n==748 else b+0x8000+len(allocations)*0x1000
  if n==748 and fail_object:
   cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp));return
  allocations.append((result,n));cpu.mem_write(result,bytes([0xa5])*n)
 elif address==0x57360e:
  assert arg(0) in [a for a,n in allocations] and arg(0) not in freed;freed.append(arg(0))
  cpu.mem_write(arg(0),bytes([0xdd])*next(n for a,n in allocations if a==arg(0)))
 elif address==0x4ffa80:pop=4
 elif address==0x40a0e0:result=parent if has_parent else 0
 elif address==0x489fe0:
  assert arg(0)==actor and arg(1)==source+0x800 and arg(2)==kind
  put(actor+0x80,0 if fail_model else model);put(actor+0x84,0xffffffff);cpu.mem_write(actor+0x78,f(model_radius));result=0 if fail_model else model
 elif address==0x503250:
  assert arg(0)==model;result=0
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop);cpu.reg_write(UC_X86_REG_EIP,read(sp))
for address in (0x4ffa80,0x40a0e0,0x573619,0x57360e,0x489fe0,0x503250):
 u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def call(address,args=(),ecx=0):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,ecx);u.reg_write(UC_X86_REG_FPCW,0x37f)
 try:u.emu_start(address,stop,count=1000000)
 except Exception:print('failure',hex(u.reg_read(UC_X86_REG_EIP)),trace[-12:]);raise
 assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
records=[]
for case in range(32):
 previous=b+0x3000 if case%2 else 0x73d880
 put(0x73d890,previous);put(0x73d894,previous);put(previous+0x10,0x73d880);put(previous+0x14,0x73d880);put(0x73a850,case%2);put(0x73db0c,case%2)
 slot=case;node=0x73a880+slot*12;put(0x7394c0,node);put(0x7394c4,node);u.mem_write(node,w(0x7394c0,0x7394c0,slot))
 generation=case+1;put(0x708744,generation);put(0x6460e8,0);put(0x59f7e4,0xffffffff)
 radius=(-1,0,.5,2)[case%4];has_parent=bool(case%2)
 u.mem_write(parent+0x28,b'\x07');put(parent+0x1f8,17)
 descriptor=bytearray(0x98);descriptor[20:24]=f(1);descriptor[60:108]=f(1,2,3,1,0,0,0,1,0,0,0,1);descriptor[132:136]=f(radius)
 u.mem_write(params,bytes(descriptor));u.mem_write(0x649f50,f(.25,.5,2));allocations.clear();trace.clear();freed.clear()
 assert call(0x486da0,(10,0xffffffff,123,params,0x30000,case%3))==actor
 assert allocations==[(actor,748)],allocations
 assert read(actor+0x24)==10 and read(actor+0x30)==123 and read(actor+0x7c)==0x6030000,(hex(read(actor+0x24)),hex(read(actor+0x30)),hex(read(actor+0x7c)))
 assert read(actor+0x80)==0 and read(actor+0x184)==0
 assert read(actor+0x1f8)==(17 if has_parent else 1) and u.mem_read(actor+0x28,1)==bytes([7 if has_parent else 0])
 assert read(actor+0x2c)==generation*65536+slot and read(0x7394cc+slot*4)==actor
 assert read(actor+0x78)==struct.unpack('<I',f(1 if radius<=0 else radius))[0]
 # Glare-specific4153b0 unlink is real code; surround by arbitrary nodes.
 sentinel=0x5c9ba8;prev=b+0xa000 if case%4 else sentinel;nxt=b+0xb000 if case%8 else sentinel
 put(actor+0x2b8,nxt);put(actor+0x2bc,prev);put(prev+0x2b8,actor);put(nxt+0x2bc,actor)
 saved=bytes(u.mem_read(actor,748));call(0x4153b0,(actor,));after=bytearray(saved);after[0x2b8:0x2c0]=bytes(8)
 assert bytes(u.mem_read(actor,748))==after and read(prev+0x2b8)==nxt and read(nxt+0x2bc)==prev
 # Restore the family links and execute the complete generic deletion path.
 u.mem_write(actor,saved);put(prev+0x2b8,actor);put(nxt+0x2bc,actor)
 pairs=[b+0xc000+j*32 for j in range(case%5)];removed=[];kept=[]
 for j,pair in enumerate(pairs):
  first=actor if j%3==0 else parent;second=actor if j%3==1 else model
  u.mem_write(pair,w(pairs[j+1] if j+1<len(pairs) else 0,first,second))
  (removed if actor in (first,second) else kept).append(pair)
 free_pair=b+0xd000;u.mem_write(free_pair,w(0,parent,model))
 u.mem_write(0x73db28,w(pairs[0] if pairs else 0,len(pairs)));u.mem_write(0x75db30,w(free_pair,1))
 assert read(actor+0x18c)==0 and read(actor+0x268)==0
 call(0x486670,(actor,))
 def chain(head):
  out=[]
  while head:
   assert head not in out;out.append(head);head=read(head)
  return out
 assert chain(read(0x73db28))==kept and read(0x73db2c)==len(kept)
 assert chain(read(0x75db30))==list(reversed(removed))+[free_pair] and read(0x75db34)==len(removed)+1
 assert read(prev+0x2b8)==nxt and read(nxt+0x2bc)==prev
 assert freed==[actor] and read(0x7394cc+slot*4)==0 and read(0x73a850)==case%2
 assert read(previous+0x10)==0x73d880 and read(0x73d894)==previous
 records.append(dict(radius=radius,parent=has_parent,bytes=748,base=saved.hex()))
report=dict(result='PASS',cases=len(records),records=records,scope='Actual original486da0 type10 generic construction with supplied heap/string/parent services, model absent and flags30000. Real registry/body initialization; allocationflags20000 suppress ordinary room binding,4153b0 family unlink preserving other bytes, then complete486670 with real48c9f0 active/free collision-pair recycling,4153b0 family unlink,49f1d0 empty physics storage,489fc0 absent-model gate, empty emitter chain and4867b0 slot retirement/free. Nonempty physics/model/emitter resources and shared/native full deletion remain excluded.')
(root/'artifacts/glare-base-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})

