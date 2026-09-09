"""Original object handle pool instruction blocks: init, allocate, release, lookup.

No object constructors, destructors or full runtime registration are executed.
"""
import hashlib,json,struct,sys,re,subprocess
from collections import deque
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x100000);stack=base+0xf0000;stop=stack+4096
pack=lambda *x:struct.pack('<'+'I'*len(x),*x)
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def run(a,end):
 u.emu_start(a,end,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==end
u.mem_write(0x7394cc,bytes(4096));run(0x486ce9,0x486d3e)
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();xi=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xi,(len(xb)+4095)//4096*4096);x.mem_write(xi,xb);x.mem_map(base,0x100000)
mapping=(root/'build/xbox/main.map').read_text()
entries={name:int(re.search('_rf_object_registry_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('init','insert','remove','lookup')}
commands=bytearray();outputs=bytearray();registry_bytes=12300

def invoke(name,args):
 x.mem_write(stack,pack(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[name],stop,count=500000)
 assert x.reg_read(UC_X86_REG_EIP)==stop,(name,hex(x.reg_read(UC_X86_REG_EIP)))
 return x.reg_read(UC_X86_REG_EAX)
invoke('init',[base])

def shared(op,arg,status,value=0xabcdef01):
 commands.extend(pack(op,arg));outputs.extend(pack(status&0xffffffff,value))
 before=bytes(x.mem_read(base,registry_bytes)) if status else None
 if op==0:
  x.mem_write(base+0x4000,pack(0xabcdef01));actual=invoke('insert',[base,arg,base+0x4000]);got=struct.unpack('<I',x.mem_read(base+0x4000,4))[0]
 elif op==1:actual=invoke('remove',[base,arg]);got=value
 elif op==2:actual=0;got=invoke('lookup',[base,arg])
 else:x.mem_write(base+12296,pack(arg));actual=0;got=value
 assert actual==status&0xffffffff and got==value,(op,arg,actual,status,got,value)
 if before is not None:assert bytes(x.mem_read(base,registry_bytes))==before,'failure changed registry'

free=deque(range(1024));generation=1;active={};allocations=0;releases=0;lookups=0

def check_free():
 node=read(0x7394c0);actual=[];previous=0x7394c0
 while node!=0x7394c0:
  assert len(actual)<1024 and read(node+4)==previous
  actual.append(read(node+8));previous=node;node=read(node)
 assert actual==list(free) and read(0x7394c4)==previous
 head,count,gen=struct.unpack('<3I',x.mem_read(base+12288,12))
 q=struct.unpack('<1024I',x.mem_read(base+8192,4096))
 assert count==len(free) and [q[(head+i)%1024] for i in range(count)]==actual and gen==generation

def lookup(handle,want):
 global lookups
 u.mem_write(stack,pack(stop,handle));u.reg_write(UC_X86_REG_ESP,stack);run(0x40a0e0,stop)
 assert u.reg_read(UC_X86_REG_EAX)==want;lookups+=1
 shared(2,handle,0,want)

def allocate():
 global generation,allocations
 slot=free.popleft();assert read(read(0x7394c0)+8)==slot
 obj=base+slot*0x100;u.reg_write(UC_X86_REG_EDI,slot);u.reg_write(UC_X86_REG_ESI,obj)
 run(0x486e35,0x486e92)
 handle=(generation<<16)|slot;generation+=1
 if generation>=0x752f:generation=1
 assert read(obj+0x2c)==handle and read(0x7394cc+slot*4)==obj
 assert struct.unpack('<H',u.mem_read(0x708744,2))[0]==generation
 active[slot]=(handle,obj);allocations+=1;shared(0,obj,0,handle);lookup(handle,obj);return slot,handle

def release(slot):
 global releases
 old,obj=active.pop(slot);u.reg_write(UC_X86_REG_ESI,slot)
 u.mem_write(stack,pack(0,stop));u.reg_write(UC_X86_REG_ESP,stack);run(0x48684f,stop)
 free.append(slot);assert read(0x7394cc+slot*4)==0;shared(1,old,0);lookup(old,0);shared(1,old,-3);releases+=1
 return old
check_free()
for _ in range(1024):allocate()
check_free();assert read(0x7394c0)==0x7394c0 # Constructor's exhaustion gate.
shared(0,base+0x80000,-4)
stale={}
for slot in (700,3,1023,0,510):stale[slot]=release(slot)
check_free()
for _ in range(5):
 slot,new=allocate();assert new!=stale[slot];lookup(stale[slot],0)
check_free()
for seed in (0x752d,0x752e,1):
 release(10);generation=seed;u.mem_write(0x708744,struct.pack('<H',seed));shared(3,seed,0);allocate();check_free()
lookup(0xffffffff,0);lookup(0x10000400,0)
shared(0,0,-4)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_object_registry_probe.exe')],input=commands)
assert raw==outputs,'PC registry differs from original trace'
report=dict(result='PASS',allocations=allocations,releases=releases,lookups=lookups,capacity=1024,generation_range=[1,0x752e],shared_operations=len(commands)//8,scope='PC and compiled NXDK registry match original outputs. Shared errors preserve registry/output; repeated removal rejects stale handles. Unchanged original initialization 486ce9..486d3e, allocation 486e35..486e92, release 48684f..486895 and lookup 40a0e0. FIFO reuse, full pool, stale handles and boundary generation seeds; constructor/destructor side effects and complete registry integration excluded.')
(root/'artifacts/object-handle-pool-verification.json').write_text(json.dumps(report,indent=2));print(report)
