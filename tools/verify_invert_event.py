"""Execute original Invert common activation and virtual actions unchanged."""
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
trace=[];mutation=0
def hook(cpu,address,size,unused):
 if address not in (0x4b65c0,0x4b6640):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(sp);on=int(address==0x4b65c0)
 trace.append([read(sp+4),read(sp+8),read(sp+12),on,0 if on else read(sp+16)])
 if mutation:write(base+0x2ac,read(base+0x2ac)^0x12345678)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);results=[]
# Type 3 takes the generic constructor, whose actual virtual table is used.
assert struct.unpack_from('<I',b,0xb7610+b[0xb76f3]*4)[0]==0x4b75ca
assert struct.unpack_from('<2I',b,0x189ca0)==(0x4b9070,0x4b9f80)
for count,mode,flags,source,change in itertools.product((0,1,3,8),(0,1,2,257),(0,1),(0,7,0xffffffff),(0,1)):
 mutation=change;u.mem_write(base,bytes(0x400));write(base,0x589c9c)
 write(base+0x290,3,0,-1,count,count,links);write(base+0x2b0,flags)
 write(links,*range(100,108));write(0x5a3ed8,12345);trace.clear()
 write(stack,stop,source,0x76543210,mode);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
 u.emu_start(0x4b8b70,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+16
 wanted=[];current=source
 if not flags:
  for i in range(count):
   wanted.append([100+i,current,0xffffffff,int((mode&255)!=1),0])
   if change:current^=0x12345678
 assert trace==wanted and read(base+0x2ac)==current
 assert read(base+0x2a8)==0x76543210 and read(base+0x298)==0xffffffff
 results.append(dict(count=count,mode=mode,flags=flags,source=source,mutation=change,trace=list(trace)))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Unchanged original common activation, base virtual dispatch, Invert on 4b9930/off 4ba330 and array helpers. Generic target effects intercepted; source mutation verifies per-link reread. No original recursive target effects or full campaign equivalence.',results=results)
(root/'artifacts/invert-event-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['result'],report['cases'])
