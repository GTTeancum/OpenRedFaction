"""Execute original visibility methods; isolate resource/AI effect boundaries."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);obj=base;child=base+0x2000
node=base+0x3000;component=base+0x4000;array=base+0x5000;stack=base+0xe000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];resource_type=0
# These boundaries are recorded, not emulated. Their internal mutations are
# deliberately excluded from the complete local-buffer comparison.
boundaries={0x505b50:2,0x429770:1,0x41ae70:2,0x42ed20:2,0x502b00:1,0x503390:3,0x48c9a0:1}
def hook(cpu,address,size,data):
 if address not in boundaries:return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 trace.append((address,tuple(read(sp+4+i*4) for i in range(boundaries[address]))))
 cpu.reg_write(UC_X86_REG_EAX,resource_type if address==0x502b00 else 0)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
cases=0
for unhide in (False,True):
 for kind in (0,1):
  for flags in (0,0x4000,0x8000,0xc000,0x1234c567):
   for populated in (False,True):
    for resource_type in (0,3):
     u.mem_write(base,bytes(0x6000))
     u.mem_write(0x7394cc,pack(obj,child))
     u.mem_write(obj+0x24,pack(kind));u.mem_write(obj+0x2c,pack(0x12340000));u.mem_write(obj+0x7c,pack(flags))
     u.mem_write(child+0x24,pack(4));u.mem_write(child+0x2c,pack(0x12340001));u.mem_write(child+0x7c,pack(flags & 0x4000))
     u.mem_write(obj+0x804,pack(77 if populated else 0xffffffff))
     u.mem_write(obj+0x145c,pack(0x12340001 if populated else 0xffffffff,0x99990001))
     u.mem_write(obj+0x2a4,pack(42))
     if populated:
      u.mem_write(obj+0x80,pack(88));u.mem_write(obj+0x13d8,pack(99))
      u.mem_write(obj+0x268,pack(node));u.mem_write(node+0x150,pack(node+0x200))
      u.mem_write(node+0x140,bytes([0xa5]));u.mem_write(node+0x340,bytes([0xa5]))
      u.mem_write(obj+0x1418,pack(2,2,array));u.mem_write(array,pack(component,component+0x400))
      u.mem_write(component+0x28c,bytes([0xa5]));u.mem_write(component+0x68c,bytes([0xa5]))
     expected=bytearray(u.mem_read(base,0x6000));calls=[]
     def put(offset,value):expected[offset:offset+4]=pack(value)
     put(0x7c,flags & ~0xc000 if unhide else flags|0x4000)
     if kind==0:
      if populated:
       for offset in (node-base+0x140,node-base+0x340,component-base+0x28c,component-base+0x68c):expected[offset]=int(unhide)
       calls.append((0x505b50,(77,0x3f800000 if unhide else 0)))
      if unhide:calls.append((0x429770,(obj,)))
      if populated:put(child-base+0x7c,(flags & 0x4000) & ~0x4000 if unhide else (flags & 0x4000)|0x4000)
      if not unhide:
       calls.append((0x41ae70,(0x12340000,42)))
       if populated:calls.append((0x42ed20,(99,0)));put(0x13d8,0)
     if unhide:
      if populated:
       calls.append((0x502b00,(88,)))
       if resource_type==3:calls.append((0x503390,(88,0,0x3f800000)))
      if flags&0x8000:calls.append((0x48c9a0,(obj,)))
     trace.clear();u.mem_write(stack,pack(stop,obj));u.reg_write(UC_X86_REG_ESP,stack)
     u.emu_start(0x48a660 if unhide else 0x48a570,stop,count=100000)
     assert u.reg_read(UC_X86_REG_EIP)==stop
     assert trace==calls,(unhide,kind,flags,populated,resource_type,trace,calls)
     actual=bytes(u.mem_read(base,0x6000))
     assert actual==expected,(unhide,kind,flags,populated,[hex(i) for i,(a,e) in enumerate(zip(actual,expected)) if a!=e][:12])
     cases+=1
for entry in (0x48a570,0x48a660):
 trace.clear();u.mem_write(stack,pack(stop,0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and trace==[]
report=dict(result='PASS',cases=cases,null_cases=2,scope='Complete original hide/unhide with real registry/type lookup, recursive child visibility, attachment chain and component array toggles. Seven downstream resource/AI boundaries intercepted; ordered arguments and all 24 KiB fixture bytes compared. No implementation of intercepted effects or shared runtime integration.')
(root/'artifacts/visibility-effects-verification.json').write_text(json.dumps(report,indent=2));print(report)
