"""Execute original light registry append and frame dispatch; no host input."""
import hashlib,json,random,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe'
sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;S=B+0xe000;STOP=B+0xff00;REG=0x646098
u.mem_map(B,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
observed=[];mutation=None

def timer(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 observed.append((cpu.reg_read(UC_X86_REG_ECX),read(sp+4)))
 if mutation and len(observed)==1:u.mem_write(REG,w(mutation))
 cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8)
u.hook_add(UC_HOOK_CODE,timer,begin=0x45fa30,end=0x45fa30)
rng=random.Random(0x646098);cases=0;visits=0
for count in [0,1,2,231,934]+[rng.randrange(1,1000) for _ in range(123)]:
 values=[B+0x5000+i*4 for i in range(count+3)]
 u.mem_write(REG,w(0,1100,B));u.mem_write(B,bytes(4400))
 # Execute actual append with existing capacity: allocation itself is excluded.
 for value in values:
  u.mem_write(S,w(STOP,value));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ECX,REG)
  u.emu_start(0x45ec40,STOP,count=1000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 assert read(REG)==len(values) and bytes(u.mem_read(B,len(values)*4))==w(*values)
 for mode in range(3):
  mutation=None if mode==0 or count==0 else (count+3 if mode==1 else 1)
  expected=count if mutation is None else mutation
  u.mem_write(REG,w(count));seconds=struct.unpack('<I',struct.pack('<f',rng.random()))[0]
  u.mem_write(0x5a4014,w(seconds));observed.clear();u.mem_write(S,w(STOP));u.reg_write(UC_X86_REG_ESP,S)
  u.emu_start(0x43332b,0x433363,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==0x433363
  assert observed==[(v,seconds) for v in values[:expected]],(count,mode,observed)
  assert u.reg_read(UC_X86_REG_ESP)==S
  cases+=1;visits+=len(observed)
result=dict(result='PASS',cases=cases,visits=visits,original_sha256=sha,scope='Actual 45ec40 append with preallocated capacity and 43332b..433363 frame loop. Timer body supplied only to observe dispatch and mutate count. File loader ordering is static evidence; frame gates, allocation growth and surrounding simulation are excluded. Timer arithmetic has separate original/PC/NXDK verification.')
(root/'artifacts/level-light-order.json').write_text(json.dumps(result,indent=2));print(result)
