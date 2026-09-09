"""Original deferred UnHide timer and request ordering with target effects intercepted."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;obj=base+0x2000;array=base+0x1000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
query_result=0;query_calls=0;factory_result=base
def hook(cpu,address,size,data):
 global query_calls
 if address==0x4b6870:
  sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==0x123 and read(sp+8)==50
  cpu.reg_write(UC_X86_REG_EAX,factory_result)
  cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp));return
 if address==0x498e80:
  sp=cpu.reg_read(UC_X86_REG_ESP)
  assert [read(sp+x) for x in (4,8,12,16)]==[base+0x4000+0x7d4,obj+0x3c,3,0]
  query_calls+=1;cpu.reg_write(UC_X86_REG_EAX,query_result)
  cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp));return
 if address not in (0x48a660,0x48a570):return
 sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==obj
 trace.append('unhide' if address==0x48a660 else 'hide')
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook);u.mem_write(0x7394cc,pack(obj));u.mem_write(obj+0x2c,pack(0x12340000));u.mem_write(0x5cb054,pack(0))
now=1000;u.mem_write(0x5a3ed8,pack(now));cases=0
for on in (0,1,2):
 for off in (0,1,2):
  for deadline in (0xffffffff,now-1,now,now+1):
   for links in ([],[0x12340000],[0x12340000,0x99990000,0x12340000]):
    u.mem_write(base,bytes(0x2c0));u.mem_write(base+0x29c,pack(len(links),len(links),array));u.mem_write(array,pack(*links))
    u.mem_write(base+0x2b8,pack(deadline));u.mem_write(base+0x2bc,bytes([on,off]));trace.clear()
    u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x4bcdf0,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    due=deadline!=0xffffffff and deadline<=now;expected=[];new_on=on;new_off=off;new_deadline=deadline
    if due and on==1:expected=['unhide']*links.count(0x12340000);new_on=0;new_deadline=now+500
    elif due and off==1:expected=['hide']*links.count(0x12340000);new_off=0;new_deadline=now+500
    assert trace==expected and read(base+0x2b8)==new_deadline and bytes(u.mem_read(base+0x2bc,2))==bytes([new_on,new_off]),(on,off,deadline,links,trace)
    cases+=1
# Preserve state across ticks: simultaneous requests must not lose the pending
# hide, and a second unhide request must wait for the shared cooldown.
u.mem_write(base,bytes(0x2c0));u.mem_write(base+0x29c,pack(1,1,array));u.mem_write(array,pack(0x12340000))
u.mem_write(base+0x2b8,pack(1000));u.mem_write(base+0x2bc,bytes([1,1]))
steps=[(1000,False,['unhide'],1500,0,1),
       (1499,False,[],1500,0,1),
       (1500,False,['hide'],2000,0,0),
       (1999,True,[],2000,1,0),
       (2000,False,['unhide'],2500,0,0)]
for clock,request,expected,deadline,on,off in steps:
 u.mem_write(0x5a3ed8,pack(clock))
 if request:u.mem_write(base+0x2bc,bytes([1]))
 trace.clear();u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4bcdf0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert trace==expected and read(base+0x2b8)==deadline and bytes(u.mem_read(base+0x2bc,2))==bytes([on,off]),(clock,trace)
player_cases=0
u.mem_write(0x5cb054,pack(base+0x4000));u.mem_write(0x5a3ed8,pack(1000))
for kind in (0,1):
 for friendliness in (0,2):
  for name in ('ordinary','masako_fighter','capek','gryphon','CAPEK','capek_extra'):
   for global_value in (0,1):
    for query_result in (0,1):
     u.mem_write(obj+0x24,pack(kind));u.mem_write(obj+0x1f8,pack(friendliness))
     raw=name.encode();u.mem_write(base+0x3000,raw+b'\0');u.mem_write(obj+0x18,pack(len(raw),base+0x3000))
     u.mem_write(0x645320,pack(global_value));u.mem_write(base+0x2b8,pack(1000));u.mem_write(base+0x2bc,bytes([1,1]))
     trace.clear();query_calls=0;u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
     u.emu_start(0x4bcdf0,stop,count=100000)
     bypass=kind!=0 or friendliness==2 or name.lower() in ('masako_fighter','capek','gryphon') or global_value!=0
     allowed=bypass or query_result!=0
     assert u.reg_read(UC_X86_REG_EIP)==stop
     assert query_calls==int(not bypass),(name,query_calls,bypass)
     assert trace==(['unhide'] if allowed else []) and read(base+0x2b8)==1500 and bytes(u.mem_read(base+0x2bc,2))==bytes([0 if allowed else 1,1]),(kind,friendliness,name,global_value,query_result,trace)
     player_cases+=1
# Execute the type-specific factory overlay, intercepting only the generic
# allocation/factory boundary. All unrelated patterned bytes must survive.
factory_cases=0
for factory_result in (0,base):
 for fill in (0,0xa5):
  for now in (0,1000,100000):
   before=bytes([fill])*0x2c0;u.mem_write(base,before);u.mem_write(0x5a3ed8,pack(now))
   u.mem_write(stack,pack(stop,0x123));u.reg_write(UC_X86_REG_ESP,stack)
   u.emu_start(0x4b8880,stop,count=100000)
   expected=bytearray(before)
   if factory_result:expected[0x2b8:0x2be]=pack(now)+bytes(2)
   assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==factory_result
   assert bytes(u.mem_read(base,0x2c0))==bytes(expected),(factory_result,fill,now)
   factory_cases+=1
report=dict(result='PASS',cases=cases,sequence_steps=len(steps),player_cases=player_cases,factory_cases=factory_cases,scope='Complete original 4bcdf0 with real timer, array, string comparison, global predicate and handle lookup. Downstream hide/unhide and 498e80 query intercepted. Player branch routing, named exceptions and request retention verified; actual query/visibility effects excluded. Complete 4b8880 overlay with generic factory intercepted verifies timer arming and request initialization.')
(root/'artifacts/unhide-deferred-verification.json').write_text(json.dumps(report,indent=2));print(report)
