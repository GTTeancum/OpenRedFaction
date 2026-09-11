"""Original408ec0 and4fa3b0 timer behavior with real clock/RNG callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;thread=b+0x6000;period=1072800000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(im)+4095)//4096*4096);m.mem_write(origin,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entries={7:entry('rf_timer_pending'),8:entry('rf_timer_set_random')};draws=0

def supply_thread(m,address,size,context):
 global draws
 if address!=0x577eef:return
 draws+=1;sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 m.reg_write(UC_X86_REG_EAX,thread);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,supply_thread)
rng=random.Random(0x4fa3b0);commands=[];expected=[];pending=0
clocks=[0,1,period//2-1,period//2,period//2+1,period-1,period]
deadlines=[-2147483648,-2,-1,0,1,period//2-1,period//2,period//2+1,period-1,period]
ranges=[(1000,2000),(0,0),(-1,-1),(-period,period),(-100,100),(period,period),(1,32768),(0,1)]
for op in (7,8):
 for i in range(2048):
  now=clocks[i%len(clocks)] if i<512 else rng.randrange(period+1)
  deadline=deadlines[(i//len(clocks))%len(deadlines)] if i<512 else rng.randrange(-1,period+1)
  low,high=ranges[i%len(ranges)];seed=rng.getrandbits(32)
  if op==7:low=high=0;seed=0
  before=bytearray(b'\xa5'*0x800);before[0x274:0x278]=w(deadline);u.mem_write(b,bytes(before));u.mem_write(0x5a3ed8,w(now));u.mem_write(thread+20,w(seed));draws=0
  u.mem_write(stack,w(stop,b) if op==7 else w(stop,low,high));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b+0x274)
  u.emu_start(0x408ec0 if op==7 else 0x4fa3b0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
  value=(u.reg_read(UC_X86_REG_EAX)&255) if op==7 else struct.unpack('<I',u.mem_read(thread+20,4))[0]
  new_deadline=struct.unpack('<I',u.mem_read(b+0x274,4))[0];before[0x274:0x278]=w(new_deadline)
  assert bytes(u.mem_read(b,0x800))==before and draws==(op==8)
  if op==7:pending+=bool(value);assert new_deadline==(deadline&0xffffffff)
  commands.append(w(now,low,high,deadline,op,seed));wanted=w(0,now,low,high,new_deadline,value);expected.append(wanted)
  x.mem_write(b,w(deadline,seed,123));args=w(stop,deadline,now,b+8) if op==7 else w(stop,b,now,low,high,b+4)
  x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[op],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
  output=bytes(x.mem_read(b,4)) if op==8 else w(deadline)
  result=bytes(x.mem_read(b+4 if op==8 else b+8,4))
  assert w(x.reg_read(UC_X86_REG_EAX),now,low,high)+output+result==wanted,('NXDK',op,i)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_timer_probe.exe')],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
# Port guards preserve deadline and RNG.
guards=[(-1,1000,2000),(period+1,1000,2000),(0,2,1),(0,-period-1,0),(0,0,period+1)]
for now,low,high in guards:
 wire=w(now,low,high,123,8,0x12345678)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_timer_probe.exe')],input=wire)
 assert struct.unpack('<I',pc[:4])[0]!=0 and pc[16:]==w(123,0x12345678)
 x.mem_write(b,w(123,0x12345678));x.mem_write(stack,w(stop,b,now,low,high,b+4));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[8],stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b,8))==w(123,0x12345678)
pending_guards=[(0,-1),(0,period+1),(period+1,0)]
for deadline,now in pending_guards:
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_timer_probe.exe')],input=w(now,0,0,deadline,7,0))
 assert struct.unpack('<I',pc[:4])[0]!=0 and pc[16:]==w(deadline,123)
 x.mem_write(b,w(123));x.mem_write(stack,w(stop,deadline,now,b));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[7],stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b,4))==w(123)
pointer_guards=[(7,w(stop,0,0,0)),(8,w(stop,0,0,1000,2000,b+4)),(8,w(stop,b,0,1000,2000,0)),(8,w(stop,b,0,1000,2000,b))]
for op,args in pointer_guards:
 x.mem_write(b,w(123,0x12345678));x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[op],stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b,8))==w(123,0x12345678)
report=dict(result='PASS',pending_cases=2048,pending_true=pending,random_timer_cases=2048,port_guards=len(guards)+len(pending_guards),nxdk_pointer_guards=len(pointer_guards),scope='Full408ec0 with actual40a0d0/4fa3f0 and full4fa3b0 with actual57312d/4fa360. Only CRT thread-storage address supplied. Exact PC/NXDK result/deadline/RNG; wrap, half-period ties, disabled timers, inclusive/equal/negative ranges and full untouched original object bytes. Shared campaign call ordering not established.')
(root/'artifacts/pain-timers.json').write_text(json.dumps(report,indent=2));print(report)
