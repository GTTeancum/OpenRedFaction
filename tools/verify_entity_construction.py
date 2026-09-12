"""Original entity constructor and allocator, with only heap allocation supplied."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);u.mem_map(0,4096)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;sentinel=0x73d880
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
zero_ranges=[(0x18,0x20),(0x184,0x190),(0x4ac,0x4b8),(0x5cc,0x5d9),(0x614,0x620),(0x648,0x655),(0x690,0x69c),(0x8cc,0x8e4),(0x1418,0x1424)]
inactive_ranges=[(0x4b8,0x4c8),(0x4cc,0x4e0),(0x4f0,0x500),(0x504,0x508),(0x514,0x51c),(0x534,0x538),(0x540,0x54c),(0x570,0x574),(0x57c,0x584),(0x6a4,0x6a8),(0x6bc,0x6c0),(0x72c,0x730),(0x744,0x748),(0x754,0x75c),(0x764,0x768),(0x770,0x78c),(0x798,0x7a0),(0x818,0x81c),(0x830,0x834),(0x8bc,0x8c0),(0x136c,0x1378),(0x137c,0x1380),(0x139c,0x13a8),(0x13cc,0x13d0),(0x13fc,0x1400),(0x140c,0x1410),(0x1414,0x1418),(0x1458,0x145c),(0x1490,0x1494)]
def constructor_expected(fill):
 result=bytearray([fill])*0x1494
 for ranges,value in ((zero_ranges,0),(inactive_ranges,255)):
  for start,end in ranges:result[start:end]=bytes([value])*(end-start)
 return result
allocation=base;calls=0
def hook(cpu,address,size,data):
 global calls
 if address!=0x573619:return
 sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==0x1494;calls+=1
 cpu.reg_write(UC_X86_REG_EAX,allocation);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
constructor_cases=allocator_cases=0
preserved_movement_fields=(0x75c,0x7b4)
preserved_movement_checks=0
for fill in (0,0xa5,0x5a):
 u.mem_write(base,bytes([fill])*0x1494);u.mem_write(0,pack(0));u.mem_write(stack,pack(stop))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x40e380,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==base and read(0)==0
 assert bytes(u.mem_read(base,0x1494))==constructor_expected(fill)
 for offset in preserved_movement_fields:
  assert read(base+offset)==int.from_bytes(bytes([fill])*4,"little")
  preserved_movement_checks+=1
 constructor_cases+=1
 for allocation in (0,base):
  for count,peak in ((0,0),(7,10)):
   before=bytes([fill])*0x1494;u.mem_write(base,before);u.mem_write(0,pack(0));u.mem_write(sentinel+0x10,pack(sentinel,sentinel))
   u.mem_write(0x73a850,pack(count));u.mem_write(0x73db0c,pack(peak));calls=0
   u.mem_write(stack,pack(stop,0,0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x487100,stop,count=100000)
   assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==allocation and calls==1 and read(0)==0
   expected=constructor_expected(fill) if allocation else bytearray(before)
   if allocation:
    for offset,value in ((0,0),(0x10,sentinel),(0x14,sentinel),(0x268,0),(0x27c,0)):expected[offset:offset+4]=pack(value)
   assert bytes(u.mem_read(base,0x1494))==expected,(fill,allocation)
   assert read(0x73a850)==count+bool(allocation) and read(0x73db0c)==max(peak,count+bool(allocation))
   assert read(sentinel+0x10)==(base if allocation else sentinel) and read(sentinel+0x14)==(base if allocation else sentinel)
   for offset in preserved_movement_fields:
    assert read(base+offset)==int.from_bytes(bytes([fill])*4,"little")
    preserved_movement_checks+=1
   allocator_cases+=1
u.mem_write(sentinel+0x10,pack(sentinel,sentinel));u.mem_write(0x73a850,pack(0));u.mem_write(0x73db0c,pack(0))
objects=[]
for allocation in (base,base+0x2000,base+0x4000):
 u.mem_write(allocation,bytes([0xa5])*0x1494);u.mem_write(stack,pack(stop,0,0));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x487100,stop,count=100000);objects.append(allocation)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==allocation
 assert read(sentinel+0x10)==objects[0] and read(sentinel+0x14)==objects[-1]
 assert read(0x73a850)==len(objects) and read(0x73db0c)==len(objects)
 for index,address in enumerate(objects):
  expected=constructor_expected(0xa5)
  for offset,value in ((0,0),(0x10,objects[index+1] if index+1<len(objects) else sentinel),(0x14,objects[index-1] if index else sentinel),(0x268,0),(0x27c,0)):expected[offset:offset+4]=pack(value)
  assert bytes(u.mem_read(address,0x1494))==expected,(index,len(objects))
report=dict(result='PASS',preserved_movement_fields=[hex(v) for v in preserved_movement_fields],preserved_movement_checks=preserved_movement_checks,constructor_cases=constructor_cases,allocator_cases=allocator_cases,insertion_steps=len(objects),bytes=0x1494,zero_ranges=zero_ranges,inactive_ranges=inactive_ranges,scope='Complete original 40e380 and type-0 487100 path, original callees unchanged except heap boundary 573619. Full object-byte comparisons, sequential tail insertion and count/high-water updates; failures preserve state. No generic 486da0 initialization, class/asset factory remainder, registry-handle assignment or gameplay creation.')
(root/'artifacts/entity-construction-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if not k.endswith('_ranges')})
