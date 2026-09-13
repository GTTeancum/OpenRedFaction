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
  n=arg(0);assert n in (728,384),n
  result=actor if n==728 else b+0x8000+len(allocations)*0x1000
  if n==728 and fail_object:
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
examples=[]
for case in range(256):
 previous=b+0x3000 if case%2 else 0x73d880
 put(0x73d890,previous);put(0x73d894,previous);put(previous+0x10,0x73d880);put(previous+0x14,0x73d880);put(0x73a850,case%2);put(0x73db0c,case%2)
 slot=case%1024;node=0x73a880+slot*12;put(0x7394c0,node);put(0x7394c4,node);u.mem_write(node,w(0x7394c0,0x7394c0,slot))
 generation=0x752e if case%7==0 else case+1;put(0x708744,generation);put(0x6460e8,0);put(0x59f7e4,0xffffffff)
 radius=(-1,0,.5,2)[case%4];count=(0,1,2,4)[(case//4)%4];kind=(1,3)[(case//16)%2]
 allocation_flags=(0,0x4000,0x10000,0x100000)[(case//32)%4];has_parent=case>=128
 model_radius=1.25;position=[1,2,3];basis=[1,0,0,0,1,0,0,0,1]
 u.mem_write(parent+0x28,b'\x07');put(parent+0x1f8,17)
 descriptor=bytearray(0x98);descriptor[0:8]=w(source+0x800,kind);descriptor[16:20]=w(2)
 descriptor[60:108]=f(*position,*basis);descriptor[132:136]=f(radius)
 descriptor[136:148]=w(count,count,source if count else 0);descriptor[148:152]=w(0x20)
 u.mem_write(params,bytes(descriptor));u.mem_write(source,b''.join(f(j,0,0,.5,-1)+w(j+7) for j in range(count)) or bytes(24));u.mem_write(source+0x800,b'model.v3m\0')
 u.mem_write(0x649f50,f(.25,.5,2));allocations.clear();trace.clear();freed.clear()
 room=case%3
 assert call(0x486da0,(4,0xffffffff,123,params,allocation_flags,room))==actor
 assert allocations[0]==(actor,728) and len(allocations)==1+(0 if allocation_flags&0x10000 else 1+int(count==0)),allocations
 assert read(actor)==room and bytes(u.mem_read(actor+4,12))==f(*position)
 assert read(0x7394cc+slot*4)==actor and read(actor+0x2c)==generation*65536+slot
 assert read(0x708744)==(1 if generation==0x752e else generation+1)
 assert read(0x7394c0)==read(0x7394c4)==0x7394c0 and read(node)==read(node+4)==0
 assert read(0x73a850)==1+case%2
 assert read(actor+0x10)==0x73d880 and read(actor+0x14)==previous and read(previous+0x10)==read(0x73d894)==actor
 flags=allocation_flags|0x6400000|(0x8000 if allocation_flags&0x4000 else 0)
 assert read(actor+0x7c)==flags and read(actor+0x34)==0x42c80000 and read(actor+0x80)==model
 assert read(actor+0x24)==4 and read(actor+0x30)==123 and read(actor+0x1fc)==2
 assert read(actor+0x200)==0xffffffff,hex(read(actor+0x200))
 assert read(actor+0x1f8)==(17 if has_parent else 1) and u.mem_read(actor+0x28,1)==bytes([7 if has_parent else 0])
 assert read(params+132)==struct.unpack('<I',f(model_radius if radius<0 else radius))[0]
 assert read(params+148)==(0 if allocation_flags&0x10000 else 0x20)
 assert read(actor+0x78)==struct.unpack('<I',f(model_radius))[0]
 assert read(actor+0x184)==(0 if allocation_flags&0x10000 else max(1,count))
 assert trace.count(0x503250)==int(count==0)
 if len(examples)<16:examples.append(dict(radius=radius,physics_radius=struct.unpack('<f',u.mem_read(actor+0x180,4))[0],spheres=read(actor+0x184)))
# Early allocation failures must not consume a slot or publish a list entry.
for empty in (False,True):
 put(0x7394c0,0x7394c0 if empty else node);put(0x7394c4,0x7394c0 if empty else node)
 u.mem_write(node,w(0x7394c0,0x7394c0,slot));put(0x7394cc+slot*4,0)
 put(0x73d890,0x73d880);put(0x73d894,0x73d880);put(0x73a850,0);put(0x708744,123)
 fail_object=not empty;allocations.clear();trace.clear();freed.clear()
 assert call(0x486da0,(4,0xffffffff,123,params,0,1))==0
 assert read(0x7394cc+slot*4)==0 and read(0x708744)==123 and read(0x73a850)==0 and read(0x73d894)==0x73d880
 assert not allocations and trace==([] if empty else [0x573619]),trace
# Model failure executes actual4867b0/48b390/40e200 rollback after publication.
fail_object=False;fail_model=True
for case in range(64):
 slot=case;node=0x73a880+slot*12;other=0x73a880+(slot+1)*12
 extra=case%2;previous=b+0x3000 if case%4>=2 else 0x73d880;old_count=int(previous!=0x73d880)
 put(0x73d890,previous);put(0x73d894,previous);put(previous+0x10,0x73d880);put(previous+0x14,0x73d880);put(0x73a850,old_count)
 put(0x7394c0,node);put(0x7394c4,other if extra else node)
 u.mem_write(node,w(other if extra else 0x7394c0,0x7394c0,slot))
 if extra:u.mem_write(other,w(0x7394c0,node,slot+1))
 put(0x7394cc+slot*4,0);generation=0x752e if case%7==0 else case+1;put(0x708744,generation)
 put(0x59f7e4,0xffffffff);allocations.clear();trace.clear();freed.clear()
 assert call(0x486da0,(4,0xffffffff,123,params,0,1))==0
 assert allocations==[(actor,728)] and freed==[actor],(allocations,freed)
 assert read(0x7394cc+slot*4)==0 and read(0x73a850)==old_count
 assert read(0x73d894)==previous and read(previous+0x10)==0x73d880
 assert read(0x708744)==(1 if generation==0x752e else generation+1)
 assert read(0x59f7e4)==0xfffffffe
 assert read(0x7394c0)==(other if extra else node) and read(0x7394c4)==node
 assert read(node)==0x7394c0 and read(node+4)==(other if extra else 0x7394c0)
 if extra:assert read(other)==node and read(other+4)==0x7394c0
 assert trace.index(0x489fe0)<trace.index(0x57360e) and 0x503250 not in trace
report=dict(result='PASS',cases=256,early_failure_cases=2,model_failure_cases=64,original_sha256=digest,examples=examples,scope='Full original486da0 type4 including real487100/411ac0 constructors, registry/list publication,49ec90/49f010 physics and48a160 room binding. Supplied heap, string assignment, parent lookup, model attach result/radius and zero model sphere count. Existing descriptor spheres execute; model sphere extraction, model resource loading, native Xbox remain unverified. Actual4867b0/48b390/40e200 model-failure rollback verified with0/1 remaining free slots, prior list members and generation wrap. Empty registry and object heap failure preserve registration state.')
(root/'artifacts/clutter-base-original.json').write_text(json.dumps(report,indent=2));print(report)
