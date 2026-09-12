"""SP death-start tail420b03..420bdc versus original PC/NXDK execution."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
b=0x30000000;stack=b+0xe000;stop=b+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
u=machine(original);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_death_tail_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
locations=[b+0x520,b+0x2728,b+0x78,b+0x4b8,b+0x810,b+0x148c,b+0x3000]
trace=[];facts=[];state=[]
def hook(m,a,size,data):
 native=m is x;sp=m.reg_read(UC_X86_REG_ESP);arg=0
 if native:
  if a!=b+0x5000:return
  op=get(m,sp+8);arg=get(m,sp+12);loc=[b+i*4 for i in range(7)]
 else:
  if a==0x4ff480:
   m.reg_write(UC_X86_REG_EAX,state[6]);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,get(m,sp));return
  if a not in (0x4096f0,0x429ab0,0x43e9b0,0x502b10):return
  op=(0x4096f0,0x429ab0,0x43e9b0,0x502b10).index(a);loc=locations
  if op<2:assert get(m,sp+4)==b+(0x2a0 if op==0 else 0)
  else:arg=get(m,sp+4)
 trace.extend((op,arg,get(m,loc[3])))
 if op==0:m.mem_write(loc[1],w(get(m,loc[1])^facts[1]))
 if op==1:m.mem_write(loc[4],w(get(m,loc[4])^facts[2]))
 if op==2:m.mem_write(loc[5],w(facts[3]))
 if op==3:m.mem_write(loc[5],w(99))
 target=get(m,sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,target)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x420b03);commands=[];expected=[];timers=events=releases=0
for case in range(1024):
 radius=rng.choice([0,0x40bfffff,0x40c00000,0x40c00001,0x7fc00000,0x7f800000,0xff800000,0xbf800000])
 state=[rng.choice([0,12,13,13,14]),rng.getrandbits(32),radius,rng.randrange(1072800001),rng.getrandbits(32),rng.choice([0,0,0x1234,0x8765]),0x99887766]
 facts=[rng.choice([0,17,1072797999,1072798000,1072798400,1072800000]),rng.choice([0,32]),rng.choice([0,0x400000]),rng.choice([0,0x8899])]
 commands.append(w(*state,*facts));u.mem_write(b,bytes(0x4000));u.mem_write(b+0x294,w(b+0x2000));u.mem_write(0x64ecb9,b'\0');u.mem_write(0x5a3ed8,w(facts[0]))
 for loc,value in zip(locations,state):u.mem_write(loc,w(value))
 before=bytes(u.mem_read(b,0x4000));trace=[];u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x420b03,0x420bdc,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x420bdc
 result=b''.join(bytes(u.mem_read(a,4)) for a in locations);want=w(0)+result+w(len(trace)//3,*trace)+bytes((12-len(trace))*4);expected.append(want)
 timers+=int(get(u,locations[3])!=state[3]);events+=int(2 in trace[::3]);releases+=int(3 in trace[::3])
 after=bytearray(u.mem_read(b,0x4000))
 for a in locations:after[a-b:a-b+4]=before[a-b:a-b+4]
 assert after==before
 trace=[];x.mem_write(b,w(*state));x.mem_write(b+0x4000,w(b+0x5000,0,b+0x4100));x.mem_write(b+0x4100,w(facts[0]));x.mem_write(stack,w(stop,b,b+0x4000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,28))+w(len(trace)//3,*trace)+bytes((12-len(trace))*4)
 assert got==want,('NXDK',case)
exe=root/'build/pc/Release/rf_entity_probe.exe';assert subprocess.check_output([str(exe),'--death-tail'],input=b''.join(commands))==b''.join(expected),'PC'
report=dict(result='PASS',cases=len(commands),timers=timers,events=events,releases=releases,original_sha256=digest,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='SP420b03..420bdc, original4fa360 executes. PC/NXDK exact state, timer wrap and callback order including class/flag/model mutation, threshold neighbors, NaN and infinities. Inventory/reset/event/model release supplied. No live death dispatch or XEMU gameplay claim.')
(root/'artifacts/death-tail.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
