"""Original deferred UnHide timer and request ordering with target effects intercepted."""
import hashlib,json,struct,sys,subprocess,re
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
commands=bytearray();expected_shared=bytearray()
def capture(deadline,on,off,clock,allowed,links,operation=None):
 commands.extend(pack(deadline,on,off,clock,int(allowed),len(links) if operation is None else operation,*(links+[0]*(3-len(links)))))
 code=0
 for action in trace:code=code*4+(1 if action=='unhide' else 2)
 flags=bytes(u.mem_read(base+0x2bc,2))
 expected_shared.extend(pack(read(base+0x2b8),flags[0],flags[1],0,code))
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
    capture(deadline,on,off,now,True,links)
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
 prior_deadline=read(base+0x2b8);prior_flags=bytes(u.mem_read(base+0x2bc,2))
 trace.clear();u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4bcdf0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert trace==expected and read(base+0x2b8)==deadline and bytes(u.mem_read(base+0x2bc,2))==bytes([on,off]),(clock,trace)
 capture(prior_deadline,*prior_flags,clock,True,[0x12340000])
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
     capture(1000,1,1,1000,allowed,[0x12340000])
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
   if factory_result:
    trace.clear();capture(struct.unpack_from('<I',before,0x2b8)[0],fill,fill,now,True,[],4)
   factory_cases+=1
request_cases=0
for operation,entry in ((5,0x4bcdd0),(6,0x4bcde0)):
 for on,off in ((0,0),(1,1),(2,2),(165,165)):
  u.mem_write(base+0x2b8,pack(1234)+bytes([on,off]));trace.clear()
  u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
  u.emu_start(entry,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
  capture(1234,on,off,1000,True,[],operation);request_cases+=1
raw=subprocess.check_output([str(root/'build/pc/Release/rf_unhide_probe.exe')],input=commands)
assert raw==expected_shared,('PC mismatch',len(raw),len(expected_shared))
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
mapping=(root/'build/xbox/main.map').read_text()
entries={name:int(re.search('_rf_unhide_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('tick','init','request')}
callback=base+0x8000;xcode=0;allowed=0
def xhook(cpu,address,size,data):
 global xcode
 if address!=callback:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,context,handle,unhide=struct.unpack('<4I',cpu.mem_read(sp,16))
 processed=handle!=0x12340000 or not unhide or allowed
 if handle==0x12340000 and processed:xcode=xcode*4+(1 if unhide else 2)
 cpu.reg_write(UC_X86_REG_EAX,int(processed));cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,xhook)
for at in range(0,len(commands),36):
 deadline,on,off,now,allowed,count,*links=struct.unpack_from('<9I',commands,at)
 x.mem_write(base,pack(deadline)+bytes([on,off,0,0]));x.mem_write(array,pack(*links));xcode=0
 name='init' if count==4 else 'request' if count>=5 else 'tick'
 args=[base,now] if count==4 else [base,int(count==5)] if count>=5 else [base,now,array,count,callback,0]
 entry=entries[name];x.mem_write(stack,pack(stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 state=bytes(x.mem_read(base,8));got=state[:4]+pack(state[4],state[5],x.reg_read(UC_X86_REG_EAX),xcode)
 assert got==expected_shared[at//36*20:(at//36+1)*20],('NXDK mismatch',at//36)
report=dict(result='PASS',pc_cases=len(commands)//36,nxdk_cases=len(commands)//36,cases=cases,sequence_steps=len(steps),player_cases=player_cases,factory_cases=factory_cases,request_cases=request_cases,scope='Complete original 4bcdf0 with real timer, array, string comparison, global predicate and handle lookup. Downstream hide/unhide and 498e80 query intercepted. PC/NXDK shared scheduler state and action trace match with target eligibility supplied by callback; actual world/visibility integration excluded. Complete original 4b8880 overlay with generic factory intercepted verifies timer arming and request initialization; successful initialization and both request methods also match PC/NXDK.')
(root/'artifacts/unhide-deferred-verification.json').write_text(json.dumps(report,indent=2));print(report)
