"""Original deferred UnHide timer and request ordering with target effects intercepted."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;obj=base+0x2000;array=base+0x1000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,address,size,data):
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
report=dict(result='PASS',cases=cases,sequence_steps=len(steps),scope='Complete original 4bcdf0 with real timer, array and handle lookup; no player present, downstream hide/unhide effects intercepted. Request equality-to-one, inactive/future/due timers, on-before-off priority, 500ms shared cooldown, missing/stale/duplicate links, persistent pending requests across ticks. Player-dependent eligibility and actual visibility effects excluded.')
(root/'artifacts/unhide-deferred-verification.json').write_text(json.dumps(report,indent=2));print(report)
