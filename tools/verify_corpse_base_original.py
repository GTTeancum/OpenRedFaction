"""Execute original type-7 base allocation, pool registration and physics setup."""
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
b=0x30000000;u.mem_map(b,65536);params=b;source=b+0x2000;stack=b+0xe000;stop=b+0xf000;pool=0x708748;actor=pool
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def put(a,v):u.mem_write(a,w(v))
allocations=[];trace=[]
def hook(cpu,address,size,data):
 if address not in (0x4ffa80,0x48a160,0x40a0e0,0x573619,0x57360e):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4);trace.append((address,arg));result=0
 if address==0x573619:
  assert arg==384;result=b+0x4000+len(allocations)*0x1000;allocations.append(result);cpu.mem_write(result,bytes([0xa5])*arg)
 elif address==0x57360e:assert arg in allocations
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4+(4 if address in (0x4ffa80,0x48a160) else 0));cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
def call(address,args=(),ecx=0):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,ecx);u.reg_write(UC_X86_REG_FPCW,0x37f)
 
 try:u.emu_start(address,stop,count=1000000)
 except Exception:print('failure',hex(u.reg_read(UC_X86_REG_EIP)),trace[-12:]);raise
 assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x486da0);examples=[]
for case in range(256):
 u.mem_write(pool,bytes([0xa5])*0x5cec);call(0x48b7b0,(0,),pool);u.mem_write(actor+0x184,bytes(12))
 # Fresh object list, one available registry slot, no room search required.
 previous=b+0x3000 if case%2 else 0x73d880
 put(0x73d890,previous);put(0x73d894,previous);put(previous+0x10,0x73d880);put(previous+0x14,0x73d880);put(0x73a850,case%2);put(0x73db0c,case%2)
 slot=case%1024;node=0x73a880+slot*12;put(0x7394c0,node);put(0x7394c4,node);u.mem_write(node,w(0x7394c0,0x7394c0,slot))
 generation=0x752e if case%7==0 else 1+case;put(0x708744,generation);put(0x6460e8,0);put(0x59f7e4,0xffffffff)
 radius=(-2,0,.5,2)[case%4];count=(0,1,2,4)[(case//4)%4];mass=(0,3)[(case//16)%2];flags=0x73 if case%2 else 0x33
 position=[rng.randrange(-20,21)*.25 for _ in range(3)];basis=[1,0,0,0,1,0,0,0,1]
 spheres=b''.join(f(j*.25,j*.5,0,.5,-1)+w(j+7) for j in range(count))
 descriptor=bytearray(0x98);descriptor[12:16]=f(10);descriptor[20:24]=f(mass);descriptor[60:108]=f(*position,*basis);descriptor[132:136]=f(radius);descriptor[136:148]=w(count,count,source if count else 0);descriptor[148:152]=w(flags)
 u.mem_write(params,bytes(descriptor));u.mem_write(source,spheres or bytes(24));u.mem_write(0x649f50,f(.25,.5,2));allocations.clear();trace.clear()
 assert call(0x486da0,(7,0xffffffff,0xffffffff,params,0,0))==actor
 assert read(0x7394cc+slot*4)==actor and read(actor+0x2c)==generation*65536+slot
 assert read(0x708744)==(1 if generation==0x752e else generation+1)
 assert read(0x7394c0)==read(0x7394c4)==0x7394c0 and read(node)==read(node+4)==0
 assert read(0x73a850)==1+case%2 and read(0x70e430)==1
 assert read(actor+0x10)==0x73d880 and read(actor+0x14)==previous and read(previous+0x10)==read(0x73d894)==actor
 assert read(actor+0x7c)==0x6400000 and read(actor+0x34)==0x42c80000 and read(actor+0x80)==0
 assert read(actor+0x2cc)==0xa5a5a5a5 # base allocation preserves this tail word
 assert read(actor+0x78)==struct.unpack('<I',f(radius if radius>0 else 1))[0]
 assert read(params+132)==struct.unpack('<I',f(1 if radius<0 else radius))[0]
 raw=bytes(u.mem_read(actor+0x88,0x170));state=raw[:12]+raw[0x10:0xfc]+raw[0x108:0x128]+raw[0x138:0x148]+raw[0x15c:0x160]+raw[0x164:0x16c]
 used=read(actor+0x184);owned=bytes(u.mem_read(read(actor+0x18c),used*24))
 if not count:owned=owned[:20]+w(0)
 result=dict(handle=read(actor+0x2c),object_flags=read(actor+0x7c),model_radius=struct.unpack('<f',u.mem_read(actor+0x78,4))[0],physics_radius=struct.unpack('<f',u.mem_read(actor+0x180,4))[0],sound=read(actor+0x2cc),sphere_count=used)
 if len(examples)<16:examples.append(result)
 if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=256,original_sha256=digest,examples=examples,scope='Complete original486da0 type7 with real487100/48b870 registration and49ec90/49f010 physics. String assignment, room attachment, parent lookup and heap supplied. No model descriptor, no room search; incoming sound word retained. Radius -2/0/.5/2, mass0/3,0..4 spheres and generation wrap.')
(root/'artifacts/corpse-base-original.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='examples'})

