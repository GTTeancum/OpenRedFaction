"""Original ambient startup/tick with actual timer and slot operations."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;period=1072800000
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,b)
 m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_ambient_schedule\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(m,address,args=b''):
 m.mem_write(stack,w(stop)+args);m.reg_write(UC_X86_REG_ESP,stack)
 m.emu_start(address,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
def native(enabled,initial,now,items,slots):
 x.mem_write(base,w(base+0x3000,4,0,0));x.mem_write(base+0x3000,items);x.mem_write(base+0x4000,slots)
 status=call(x,entry,w(base,base+0x4000,enabled,now,initial))
 return w(status)+bytes(x.mem_read(base+0x3000,176))+bytes(x.mem_read(base+0x4000,600))
rng=random.Random(0x45ae30);commands=bytearray();expected=bytearray();modified=[0,0]
for case in range(2048):
 initial=case%2;enabled=(0,1,255,256,257)[case//2%5]
 now=(0,1,500,period//2,period-1,period)[case//10%6];items=bytearray()
 for i in range(4):
  slot=rng.choice((-2,-1,-1,0,24,25));sample=rng.choice((-1,0,88,2599,2600))
  delay=rng.choice((0,0,1,-1,500,-500,period,-period))
  deadline=rng.choice((-2,-1,0,now,(now+1)%(period+1),period,period//2+1))
  items.extend(w(i,sample,slot)+f(i+.25,case*.5,-3,5,.75,1)+w(delay,deadline))
 slots=bytearray(rng.randbytes(600))
 for i in range(25):slots[i*24:i*24+4]=w(-1 if case%3 and i>=24-(case%4) else i)
 u.mem_write(0x17543d8,bytes([enabled&255]));u.mem_write(0x5a3ed8,w(now));u.mem_write(0x1754170,bytes(slots))
 u.mem_write(0x644ec0,w(base+0x1000,base+0x1300))
 for i in range(4):
  raw=bytearray(b'\xa5'*60);raw[:8]=w(base+0x1000+(i+1)*0x100 if i<3 else 0x644ec0,base+0x1000+(i-1)*0x100 if i else 0x644ec0)
  raw[8:32]=items[i*44:i*44+24];raw[40:60]=items[i*44+24:(i+1)*44];u.mem_write(base+0x1000+i*0x100,bytes(raw))
 call(u,0x45ade0 if initial else 0x45ae30)
 result=bytearray(w(0))
 for i in range(4):
  raw=bytes(u.mem_read(base+0x1000+i*0x100,60));result.extend(raw[8:32]+raw[40:60])
 result.extend(u.mem_read(0x1754170,600))
 assert native(enabled,initial,now,bytes(items),bytes(slots))==result,case
 commands.extend(w(enabled,initial,now)+items+slots);expected.extend(result)
 modified[initial]+=result[4:]!=items+slots
# Explicit port-contract failures must preserve every instance and slot.
invalid=0
for bad_now,bad_initial,bad_delay,bad_deadline in [(-1,0,0,0),(period+1,0,0,0),(0,2,0,0),(0,1,period+1,0),(0,1,-period-1,0),(0,0,0,period+1)]:
 items=bytearray((w(0,88,-1)+f(0,0,0,5,1,1)+w(bad_delay,bad_deadline))*4);slots=bytes(600)
 result=w(-4)+items+slots
 assert native(1,bad_initial,bad_now,bytes(items),slots)==result
 commands.extend(w(1,bad_initial,bad_now)+items+slots);expected.extend(result);invalid+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--ambient-schedule'],input=commands)
assert actual==expected
report=dict(result='PASS',original_cases=2048,invalid_cases=invalid,modified_tick_startup=modified,original_sha256=digest,
 scope='Full original45ade0/45ae30 ordered list sweeps with unchanged slot/vector/timer callees. Entire shared instance state and600-byte table match PC/NXDK. Initial equality versus tick signed test, delay/expiry/wrap, full table and disabled audio; explicit port invalid-input preservation. Caller lifecycle and device playback excluded.')
(root/'artifacts/ambient-schedule.json').write_text(json.dumps(report,indent=2));print(report)
