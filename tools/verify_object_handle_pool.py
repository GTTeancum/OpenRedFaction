"""Original object handle pool instruction blocks: init, allocate, release, lookup.

No object constructors, destructors or full runtime registration are executed.
"""
import hashlib,json,struct,sys
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
free=deque(range(1024));generation=1;active={};allocations=0;releases=0;lookups=0

def check_free():
 node=read(0x7394c0);actual=[];previous=0x7394c0
 while node!=0x7394c0:
  assert len(actual)<1024 and read(node+4)==previous
  actual.append(read(node+8));previous=node;node=read(node)
 assert actual==list(free) and read(0x7394c4)==previous

def lookup(handle,want):
 global lookups
 u.mem_write(stack,pack(stop,handle));u.reg_write(UC_X86_REG_ESP,stack);run(0x40a0e0,stop)
 assert u.reg_read(UC_X86_REG_EAX)==want;lookups+=1

def allocate():
 global generation,allocations
 slot=free.popleft();assert read(read(0x7394c0)+8)==slot
 obj=base+slot*0x100;u.reg_write(UC_X86_REG_EDI,slot);u.reg_write(UC_X86_REG_ESI,obj)
 run(0x486e35,0x486e92)
 handle=(generation<<16)|slot;generation+=1
 if generation>=0x752f:generation=1
 assert read(obj+0x2c)==handle and read(0x7394cc+slot*4)==obj
 assert struct.unpack('<H',u.mem_read(0x708744,2))[0]==generation
 active[slot]=(handle,obj);allocations+=1;lookup(handle,obj);return slot,handle

def release(slot):
 global releases
 old,obj=active.pop(slot);u.reg_write(UC_X86_REG_ESI,slot)
 u.mem_write(stack,pack(0,stop));u.reg_write(UC_X86_REG_ESP,stack);run(0x48684f,stop)
 free.append(slot);assert read(0x7394cc+slot*4)==0;lookup(old,0);releases+=1
 return old
check_free()
for _ in range(1024):allocate()
check_free();assert read(0x7394c0)==0x7394c0 # Constructor's exhaustion gate.
stale={}
for slot in (700,3,1023,0,510):stale[slot]=release(slot)
check_free()
for _ in range(5):
 slot,new=allocate();assert new!=stale[slot];lookup(stale[slot],0)
check_free()
for seed in (0x752d,0x752e,1):
 release(10);generation=seed;u.mem_write(0x708744,struct.pack('<H',seed));allocate();check_free()
lookup(0xffffffff,0);lookup(0x10000400,0)
report=dict(result='PASS',allocations=allocations,releases=releases,lookups=lookups,capacity=1024,generation_range=[1,0x752e],scope='Unchanged original initialization 486ce9..486d3e, allocation 486e35..486e92, release 48684f..486895 and lookup 40a0e0. FIFO reuse, full pool, stale handles and boundary generation seeds; constructor/destructor side effects and complete registry integration excluded.')
(root/'artifacts/object-handle-pool-verification.json').write_text(json.dumps(report,indent=2));print(report)
