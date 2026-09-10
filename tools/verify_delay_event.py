"""Execute complete original Delay activation and expiry, intercepting targets."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,65536);links=base+0x1000;stack=base+0xe000;stop=base+0xf000
def write(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
trace=[]
def hook(cpu,address,size,unused):
 if address not in (0x4b65c0,0x4b6640):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp);on=int(address==0x4b65c0)
 trace.append([read(sp+4),read(sp+8),read(sp+12),on,0 if on else read(sp+16)])
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
assert struct.unpack_from('<I',b,0xb7610+b[0xb76f0+48]*4)[0]==0x4b75ca
results=[]
for delay,count,mode,source in itertools.product((.001,.5,5.0),(0,1,3),(0,1,2,257),(7,0xffffffff)):
 u.mem_write(base,bytes(0x400));write(base,0x589c9c)
 bits=struct.unpack('<I',struct.pack('<f',delay))[0]
 write(base+0x290,48,bits,-1,count,count,links);write(links,100,101,102)
 write(0x5a3ed8,12345);trace.clear();write(stack,stop,source,88,mode)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x4b8b70,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and not trace
 deadline=read(base+0x298);assert deadline==12345+int(delay*1000+.5)
 for now,expired in ((deadline-1,False),(deadline,True),(deadline+1,False)):
  trace.clear();write(0x5a3ed8,now);write(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
  u.emu_start(0x4b8ce0,stop,count=10000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
  on=int((mode&255)==1);wanted=[[100+i,source,88,on,1-on] for i in range(count)] if expired else []
  assert trace==wanted,(delay,count,mode,now,trace,wanted)
  assert read(base+0x298)==(deadline if now<deadline else 0xffffffff)
 results.append(dict(delay=delay,count=count,mode=mode,source=source,deadline=deadline))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Original Delay type 48 common activation, actual base virtual no-op actions, complete 4b8ce0 tick and ordered propagation. Generic target effects intercepted. Before/exact/after expiry; no repeated dispatch. Shared fixture/native integration reported separately.',results=results)
(root/'artifacts/delay-event-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['result'],report['cases'])
