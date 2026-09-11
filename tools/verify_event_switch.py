"""Original Switch on-action vs PC/NXDK with linked/audio effects observed."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_event_switch_on\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def fields(m):
 return bytes(m.mem_read(base+0x2b8,8))+bytes(m.mem_read(base+0x2c8,8))+bytes(m.mem_read(base+0x2d4,4))
def hash_row(effect,state):
 global trace
 for value in [effect]+list(struct.unpack('<5I',state)):trace=((trace^value)*16777619)&0xffffffff
counts=[0,0,0]
def original_hook(m,address,size,context):
 if address not in (0x4bc340,0x5056a0,0x505560):return
 sp=m.reg_read(UC_X86_REG_ESP);effect={0x4bc340:0,0x5056a0:1,0x505560:2}[address]
 if effect==0:assert m.reg_read(UC_X86_REG_ECX)==base and read(m,sp+4)==0
 if effect==1:assert bytes(m.mem_read(sp+4,20))==w(123,base+0x40,0x3f800000,0x173c378,0)
 if effect==2:assert bytes(m.mem_read(sp+4,16))==w(2,0,0,0x3f800000)
 hash_row(effect,fields(m));counts[effect]+=1
 m.reg_write(UC_X86_REG_EIP,read(m,sp));m.reg_write(UC_X86_REG_ESP,sp+(8 if effect==0 else 4))
def shared_hook(m,address,size,context):
 if address==base+0x2000:
  sp=m.reg_read(UC_X86_REG_ESP);assert read(m,sp+8)==base
  hash_row(read(m,sp+12),bytes(m.mem_read(base,20)))
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,shared_hook)
x.mem_write(base+0x2000,b'\xc3')
commands=bytearray();expected=bytearray();cases=0
import itertools
for disabled,limit,unlimited,count,mode in itertools.product((0,1,7,0xffffffff),(0,1,0xffffffff),(0,1,256,257),(0,1,0xffffffff,0x7fffffff), (0,1,2,3)):
 data=w(disabled,limit,unlimited,count,mode);raw=bytearray(b'\xa5'*0x2d8)
 raw[0x2b8:0x2c0]=data[:8];raw[0x2c8:0x2d0]=data[8:16];raw[0x2d0:0x2d8]=w(123,mode)
 u.mem_write(base,bytes(raw));u.reg_write(UC_X86_REG_ECX,base);u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack)
 trace=2166136261;u.emu_start(0x4bc520,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=fields(u)+w(trace);after=bytes(u.mem_read(base,len(raw)))
 assert after[:0x2b8]==raw[:0x2b8] and after[0x2bc:0x2cc]==raw[0x2bc:0x2cc] and after[0x2d0:]==raw[0x2d0:]
 commands.extend(data);expected.extend(result)
 x.mem_write(base,data);x.mem_write(stack,w(stop,base,base+0x2000,0));x.reg_write(UC_X86_REG_ESP,stack)
 trace=2166136261;x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,20))+w(trace)==result,(cases,data.hex());cases+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--switch'],input=commands)
assert actual==expected
report=dict(result='PASS',cases=cases,effect_counts=counts,original_sha256=digest,scope='Complete4bc520 state transitions. Linked update4bc340 and audio5056a0/505560 observed and replaced by no-op callbacks; original call arguments and callback state/order checked. PC/NXDK transition state and callback traces exact. Limits, low-byte unlimited, mode rejection and wrapping counts; linked effects and campaign integration excluded.')
(root/'artifacts/event-switch.json').write_text(json.dumps(report,indent=2));print(report)
