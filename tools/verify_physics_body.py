"""Original fresh body initialization compared with shared owned PC/NXDK bodies."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image);u.mem_map(0,4096)
base=0x30000000;params=base+0x2000;source=base+0x3000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
allocations=[]
def original_heap(cpu,address,size,data):
 if address not in (0x573619,0x57360e):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4)
 if address==0x573619:
  assert arg in (384,768);pointer=base+0x5000+len(allocations)*0x1000;allocations.append(pointer)
  cpu.mem_write(pointer,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,pointer)
 else:assert arg in allocations
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
u.hook_add(UC_HOOK_CODE,original_heap)
rng=random.Random(0x49f010);commands=[];expected=[]
for case in range(480):
 count=(0,1,2,16,17,32)[case%6];flags=(0,1,0x10,0x20,0x40,0x70,0x80000000,0x20000)[(case//6)%8]
 coefficients=[.25,.5,.75];mass=(0,1,2,8)[case%4]
 tensor=[rng.randrange(-16,17)*.125 for _ in range(9)];position=[rng.uniform(-100,100) for _ in range(3)]
 orientation=[rng.randrange(-8,9)*.25 for _ in range(9)]
 vectors=[rng.uniform(-10,10) for _ in range(6)]
 shared=floats(*coefficients,mass,*tensor,*position,*orientation,*vectors)+pack(flags)
 spheres=b''.join(floats(*[rng.uniform(-10,10) for _ in range(3)],rng.uniform(0,3),.25 if i==count-1 and case%2 else -1)+pack(0x12340000+i) for i in range(count))
 p=bytearray(0xa0);p[0xc:0x10]=shared[4:8];p[0x14:0x3c]=shared[12:52];p[0x3c:0x84]=shared[52:124]
 p[0x84:0x88]=floats(99);p[0x88:0x94]=pack(count,count,source);p[0x94:0x98]=pack(flags)
 u.mem_write(0x649f50,floats(coefficients[0],coefficients[2],1));u.mem_write(params,bytes(p));u.mem_write(source,spheres or bytes(24))
 u.mem_write(base,bytes([0xa5])*0x170);u.mem_write(base+0xfc,bytes(12));u.mem_write(0,pack(0));allocations.clear()
 u.mem_write(stack,pack(stop,base,params,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49f010,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and read(u,0)==0
 actual=bytes(u.mem_read(base,0x170));used=count if flags&0x70 else 0
 state=actual[:12]+actual[0x10:0xfc]+actual[0x108:0x128]+actual[0x138:0x148]+actual[0x15c:0x160]+actual[0x164:0x16c]
 assert len(state)==308 and read(u,base+0xfc)==used
 owned=bytes(u.mem_read(read(u,base+0x104),used*24)) if used else b''
 assert owned==spheres[:used*24]
 p[0x94:0x98]=actual[0x120:0x124];assert bytes(u.mem_read(params,len(p)))==p
 assert bytes(u.mem_read(source,len(spheres)))==spheres
 commands.append(shared+pack(count)+spheres);expected.append(state+pack(used)+owned)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--body'],input=b''.join(commands))
assert actual==b''.join(expected),'PC body mismatch'

xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_physics_body_open');close=symbol('rf_physics_body_close');malloc=symbol('malloc');free=symbol('free')
heap_calls=[];fail_heap=False
def shared_heap(cpu,address,size,data):
 if address not in (malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(cpu,sp+4);heap_calls.append((address,arg))
 if address==malloc:
  assert 0<arg<=32*24;cpu.mem_write(base+0x5000,bytes([0xa5])*arg);cpu.reg_write(UC_X86_REG_EAX,0 if fail_heap else base+0x5000)
 else:assert arg in (0,base+0x5000)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(cpu,sp))
x.hook_add(UC_HOOK_CODE,shared_heap)
def call(address,args):
 x.mem_write(stack,pack(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(address,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 return x.reg_read(UC_X86_REG_EAX)
heap_failure_cases=0
for i,(command,want) in enumerate(zip(commands,expected)):
 count=struct.unpack_from('<I',command,128)[0];used=struct.unpack_from('<I',want,308)[0];budget=324+used*24
 x.mem_write(params,command[:128]);x.mem_write(source,command[132:] or bytes(24));x.mem_write(base,bytes(324));heap_calls.clear()
 args=[params,source,count,budget,base]
 assert call(entry,[params,source,count,budget-1,base])==0xfffffffc
 assert bytes(x.mem_read(base,324))==bytes(324) and not heap_calls
 if used:
  fail_heap=True;assert call(entry,args)==0xfffffffc;fail_heap=False;heap_failure_cases+=1
  assert bytes(x.mem_read(base,324))==bytes(324);heap_calls.clear()
 assert call(entry,args)==0
 assert heap_calls==([(malloc,used*24)] if used else [])
 assert read(x,base+320)==budget and read(x,base+312)==used and read(x,base+316)==12+used*24
 pointer=read(x,base+308);assert pointer==(base+0x5000 if used else 0)
 snapshot=bytes(x.mem_read(base,324));assert call(entry,args)==0xfffffffc and bytes(x.mem_read(base,324))==snapshot
 x.mem_write(params,bytes([0xa5])*128);x.mem_write(source,bytes([0xa5])*max(24,count*24))
 got=bytes(x.mem_read(base,308))+pack(used)+(bytes(x.mem_read(pointer,used*24)) if used else b'')
 assert got==want,('NXDK body',i)
 call(close,[base]);assert bytes(x.mem_read(base,324))==bytes(324)
 call(close,[base]);assert bytes(x.mem_read(base,324))==bytes(324)
guards=[]
valid=commands[0][:124]+pack(0x70);one_sphere=floats(1,2,3,1,-1)+pack(0)
for offset in range(0,124,4):
 for bad in (float('inf'),float('nan')):
  invalid=bytearray(valid);invalid[offset:offset+4]=floats(bad);guards.append(bytes(invalid))
for invalid in guards:
 seed=bytes([0xa5])*308+bytes(16);x.mem_write(base,seed);x.mem_write(params,invalid);x.mem_write(source,one_sphere);heap_calls.clear()
 assert call(entry,[params,source,1,348,base])==0xfffffffc
 assert bytes(x.mem_read(base,324))==seed and not heap_calls
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),nxdk_heap_failure_cases=heap_failure_cases,nxdk_nonfinite_guard_cases=len(guards),max_accounted_body_bytes=1092,scope='Complete original fresh 49f010 with only heap allocate/free supplied; all 308 represented state bytes and owned sphere records match shared PC/NXDK. Prepared inputs, eight flag modes, 0..32 spheres, positive parameter_10 propagation. Exact/short budgets, reopening, source lifetime and repeated close checked on both builds; NXDK heap/nonfinite failures preserve owner. Original untouched fields, reinitialization, model mass generation, runtime entity binding and live Xbox execution excluded.')
(root/'artifacts/physics-body-verification.json').write_text(json.dumps(report,indent=2));print(report)
