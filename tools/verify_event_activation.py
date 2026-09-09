"""Execute original event activation/scheduling, intercepting action effects."""
import hashlib,json,struct,sys,subprocess,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;vtable=base+0x1000;on=base+0x2000;off=on+16
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
commands=bytearray();expected=bytearray()
def capture(state,tick,now,source,actor,mode):
 def fields(data):return data[0x290:0x29c]+data[0x2a8:0x2b0]+data[0x2b0:0x2b4]+pack(data[0x2b4])
 commands.extend(fields(state)+pack(tick,now,source,actor,mode))
 code=0
 for action in actions:code=code*4+{'off':1,'on':2,'propagate':3}[action]
 expected.extend(fields(bytes(u.mem_read(base,0x2c0)))+pack(0,code))
actions=[]
def hook(cpu,address,size,data):
 if address not in (on,off,0x4b8b00):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp)
 assert cpu.reg_read(UC_X86_REG_ECX)==base
 actions.append('on' if address==on else 'off' if address==off else 'propagate')
 if address==0x4b8b00:assert tuple(struct.unpack('<3I',u.mem_read(sp+4,12)))[:2]==(source,actor) and (read(sp+12)&255)==mode
 cpu.reg_write(UC_X86_REG_ESP,sp+(16 if address==0x4b8b00 else 4));cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);u.mem_write(vtable,pack(0,on,off))
source=0x12340001;actor=0x23450002;now=12345;u.mem_write(0x5a3ed8,pack(now))
scale=struct.unpack('<f',u.mem_read(0x5897b8,4))[0]
excluded=[];cases=0;counts=dict(disabled=0,delayed=0,immediate=0)
for kind in range(90):
 u.mem_write(stack,pack(stop,kind));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4b8c40,stop,count=10000)
 propagate=(u.reg_read(UC_X86_REG_EAX)&255)==1
 if not propagate:excluded.append(kind)
 for flags in (0,1):
  for delay in (-1.0,0.0,0.0004,1.0):
   for mode in (0,1,2):
    state=bytearray(0x2c0);struct.pack_into('<I',state,0,vtable);struct.pack_into('<IfI',state,0x290,kind,delay,777)
    struct.pack_into('<3I',state,0x2a8,0,0,flags);state[0x2b4]=0x55;u.mem_write(base,bytes(state))
    actions.clear();u.mem_write(stack,pack(stop,source,actor,mode));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4b8b70,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert read(base+0x2a8)==actor and read(base+0x2ac)==source
    if flags:
     assert read(base+0x298)==777 and bytes(u.mem_read(base+0x2b4,1))==b'\x55' and not actions;counts['disabled']+=1
    elif delay>0:
     dt=int(delay*(scale if kind==79 else 1)*1000+0.5)
     assert read(base+0x298)==now+dt and bytes(u.mem_read(base+0x2b4,1))==bytes([mode]) and not actions,(kind,delay,mode)
     counts['delayed']+=1
    else:
     assert read(base+0x298)==0xffffffff and actions==(['on' if mode==1 else 'off']+(['propagate'] if propagate else [])),(kind,mode,actions)
     assert bytes(u.mem_read(base+0x2b4,1))==b'\x55';counts['immediate']+=1
    capture(state,0,now,source,actor,mode)
    cases+=1
tick_cases=0
# Execute the timer/action prefix only; type-specific per-frame updates follow.
for kind in range(90):
 for mode in (0,1,2):
  for delta in (-1,0,1):
   for flags in (0,1):
    state=bytearray(0x2c0);struct.pack_into('<I',state,0,vtable)
    struct.pack_into('<IfI',state,0x290,kind,1.0,now+delta)
    struct.pack_into('<3I',state,0x2a8,actor,source,flags);state[0x2b4]=mode
    u.mem_write(base,bytes(state));actions.clear()
    u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
    u.emu_start(0x4b8ce0,0x4b8d45,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4b8d45
    if delta<=0:
     assert actions==(['on' if mode else 'off']+(['propagate'] if kind not in excluded else [])),(kind,mode,flags,actions)
     assert read(base+0x298)==0xffffffff
    else:assert not actions and read(base+0x298)==now+delta
    capture(state,1,now,source,actor,mode)
    tick_cases+=1
# Re-activation replaces one deadline, rather than adding a second queued item.
state=bytearray(0x2c0);struct.pack_into('<I',state,0,vtable)
struct.pack_into('<IfI',state,0x290,30,1.0,0xffffffff);u.mem_write(base,bytes(state))
def activate(at_time,new_source,new_actor,new_mode):
 u.mem_write(0x5a3ed8,pack(at_time));u.mem_write(stack,pack(stop,new_source,new_actor,new_mode))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4b8b70,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
activate(100,source,actor,1);assert read(base+0x298)==1100
activate(200,source+1,actor+1,0);assert read(base+0x298)==1200
assert read(base+0x2a8)==actor+1 and read(base+0x2ac)==source+1
u.mem_write(base+0x2b0,pack(1))
activate(300,source+2,actor+2,1)
assert read(base+0x298)==1200 and read(base+0x2a8)==actor+2 and read(base+0x2ac)==source+2
assert bytes(u.mem_read(base+0x2b4,1))==bytes([0])
raw=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe')],input=commands)
assert raw==expected,('PC event mismatch',next((i for i,(a,b) in enumerate(zip(raw,expected)) if a!=b),None))
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_rf_event_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('activate','tick')]
xcode=0
callback=base+0x3000
def xhook(cpu,address,size,data):
 global xcode
 if address!=callback:return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
 action=struct.unpack('<I',cpu.mem_read(sp+12,4))[0];xcode=xcode*4+action+1
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,xhook)
for at in range(0,len(commands),48):
 state=commands[at:at+28];tick,now,source,actor,mode=struct.unpack_from('<5I',commands,at+28)
 x.mem_write(base,bytes(state));xcode=0
 args=[base,now,callback,0] if tick else [base,now,source,actor,mode,callback,0]
 x.mem_write(stack,pack(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entries[tick],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=bytes(x.mem_read(base,28))+pack(x.reg_read(UC_X86_REG_EAX),xcode)
 assert got==expected[(at//48)*36:(at//48+1)*36],('NXDK',at//48,got.hex())
report=dict(result='PASS',pc_cases=len(commands)//48,nxdk_cases=len(commands)//48,cases=cases,tick_cases=tick_cases,retrigger_cases=3,counts=counts,no_automatic_propagation_types=excluded,special_delay_scale=dict(type=79,scale=scale),scope='Original 4b8b70, timer helpers, ftol and propagation predicate execute unchanged. Virtual actions and 4b8b00 link propagation intercepted; Delayed tick prefix 4b8ce0..4b8d45 executes unchanged; type-specific per-frame updates excluded. PC and compiled NXDK state/output/action-order match 3,780 cases with non-mutating callbacks. No actual event actions, callback mutation coverage or type-specific per-frame updates claimed.')
(root/'artifacts/event-activation-verification.json').write_text(json.dumps(report,indent=2));print(report)
