"""Original49ec90 creation physics modes versus owned PC/NXDK bodies."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0,4096);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');b=0x30000000;params=b+0x2000;source=b+0x3000;stack=b+0xe000;stop=b+0xf000
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
allocations=[]
def original_heap(cpu,address,size,data):
 if address not in (0x573619,0x57360e):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4)
 if address==0x573619:
  assert arg==384;pointer=b+0x5000+len(allocations)*0x1000;allocations.append(pointer);cpu.mem_write(pointer,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,pointer)
 else:assert arg in allocations, (case, hex(arg), allocations)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
for a in (0x573619,0x57360e):u.hook_add(UC_HOOK_CODE,original_heap,begin=a,end=a)
rng=random.Random(0x416a97);commands=[];expected=[]
for case in range(640):
 count=(0,1,2,4,8)[case%5];mass=(0,-1,3,100)[(case//5)%4];radius=1.25;flags=(0,0x20,0x40,0x70,0x33,0x73,0x30,0x50)[(case//20)%8]
 response=(0,10,2.5)[case%3];material=(.25,.5,2);position=[rng.randrange(-80,81)*.25 for _ in range(3)]
 basis=[1,0,0,0,1,0,0,0,1] if case%3 else [0,0,1,0,1,0,-1,0,0]
 spheres=b''.join(f(*[rng.randrange(-16,17)*.25 for _ in range(3)],rng.randrange(1,9)*.25,.25 if j==count-1 and case%2 else -1)+w(0x12340000+j) for j in range(count))
 wire=f(response,mass,*position,*basis,radius)+w(flags)+f(*material)+w(count)+spheres;commands.append(wire)
 descriptor=bytearray(0x98);descriptor[12:16]=f(response);descriptor[20:24]=f(mass);descriptor[60:108]=f(*position,*basis);descriptor[132:136]=f(radius);descriptor[136:148]=w(count,count,source if count else 0);descriptor[148:152]=w(flags)
 u.mem_write(params,bytes(descriptor));u.mem_write(source,spheres or bytes(24));u.mem_write(0x649f50,f(*material));u.mem_write(b,bytes([0xa5])*0x170);u.mem_write(b+0xfc,bytes(12));allocations.clear()
 u.mem_write(stack,w(stop,b,params));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x49ec90,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 raw=bytes(u.mem_read(b,0x170));used=read(u,b+0xfc);assert used==((count or 1) if flags&0x70 else 0)
 state=raw[:12]+raw[0x10:0xfc]+raw[0x108:0x128]+raw[0x138:0x148]+raw[0x15c:0x160]+raw[0x164:0x16c];assert len(state)==308
 owned=bytes(u.mem_read(read(u,b+0x104),used*24))
 if used and not count:owned=owned[:20]+w(0) # original fallback opaque word is undefined
 elif used:assert owned==spheres
 expected.append(w(0)+state+w(used)+owned)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--creation-body'],input=b''.join(commands));assert pc==b''.join(expected),'PC body mismatch'
mapping=(root/'build/xbox/main.map').read_text();symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_physics_creation_body_open');close=symbol('rf_physics_body_close');malloc=symbol('malloc');free=symbol('free');heap_calls=[];fail_heap=False
def heap(cpu,address,size,data):
 if address not in (malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4);heap_calls.append((address,arg))
 if address==malloc:
  assert 0<arg<=192;cpu.mem_write(b+0x5000,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,0 if fail_heap else b+0x5000)
 else:assert arg in (0,b+0x5000)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
for a in (malloc,free):x.hook_add(UC_HOOK_CODE,heap,begin=a,end=a)
def call(address,args):
 x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(address,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop;return x.reg_read(UC_X86_REG_EAX)
for case,(wire,want) in enumerate(zip(commands,expected)):
 values=struct.unpack('<20I',wire[:80]);count=values[19];used=(count or 1) if values[15]&0x70 else 0;budget=324+used*24
 x.mem_write(params,wire[:8]+wire[8:56]+wire[56:60]+w(source,count,values[15]));x.mem_write(source,wire[80:] or bytes(24));x.mem_write(b,bytes(324));heap_calls.clear()
 args=[params,*values[16:19],budget,b]
 assert call(entry,[params,*values[16:19],budget-1,b])==0xfffffffc and not heap_calls and bytes(x.mem_read(b,324))==bytes(324)
 if used:
  fail_heap=True;assert call(entry,args)==0xfffffffc;fail_heap=False;assert bytes(x.mem_read(b,324))==bytes(324);heap_calls.clear()
 assert call(entry,args)==0 and heap_calls==([(malloc,used*24)] if used else [])
 snapshot=bytes(x.mem_read(b,324));assert call(entry,args)==0xfffffffc and bytes(x.mem_read(b,324))==snapshot
 x.mem_write(params,bytes([0xa5])*72);x.mem_write(source,bytes([0xa5])*max(24,count*24))
 got=w(0)+bytes(x.mem_read(b,308))+w(read(x,b+312))+bytes(x.mem_read(read(x,b+308),used*24))
 assert got==want,('NXDK',case)
 assert read(x,b+320)==budget
 heap_calls.clear();call(close,[b]);assert heap_calls==[(free,b+0x5000 if used else 0)] and bytes(x.mem_read(b,324))==bytes(324)
 heap_calls.clear();call(close,[b]);assert all(a==free and arg==0 for a,arg in heap_calls)
report=dict(result='PASS',cases=len(commands),heap_failures=sum(1 for v in commands if struct.unpack_from("<I",v,60)[0]&0x70),budget_failures=len(commands),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original49ec90/49f010 with no geometric model vs owned PC/NXDK creation bodies. Generated/inherited mass,0..8 spheres,0/20/40/70/33/73/30/50 flags, supplied material. Original heap supplied; fallback opaque word normalized to zero. Exact Xbox budgets, zero/one owned allocation, failure preservation and close verified. No live scene/XEMU binding.')
(root/'artifacts/creation-body-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
