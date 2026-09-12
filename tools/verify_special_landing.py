"""Original special landing block; real route reset, explicit navigation/AI boundaries."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];case=0;expected=b''
def ret(cpu,value):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,address,size,data):
 global expected
 if address==0x40c2c0:
  sp=cpu.reg_read(UC_X86_REG_ESP)
  assert bytes(cpu.mem_read(sp+4,28))==w(base+0x3c,r(base+0x7c0),r(base+0x7c4),1,base+0x69c,base+0x6a0,0)
  assert bytes(cpu.mem_read(base,0x1494))==expected,'reset footprint'
  trace.append('query');cpu.mem_write(base+0x69c,w(0xabc00000+case,0xdef00000+case))
  expected[0x69c:0x6a4]=w(0xabc00000+case,0xdef00000+case)
  ret(cpu,case%2)
 elif address==0x408ac0:
  sp=cpu.reg_read(UC_X86_REG_ESP);assert r(sp+4)==base+0x2a0
  assert bytes(cpu.mem_read(base,0x1494))==expected,'AI observes query writes'
  trace.append('AI');flags=r(base+0x810)^0x400;cpu.mem_write(base+0x810,w(flags));expected[0x810:0x814]=w(flags);ret(cpu,0)
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x419981)
for case in range(512):
 seed=rng.randbytes(0x1494);expected=bytearray(seed);clock=rng.randrange(0x3ff1a101)
 for off,value in ((0,0),(0x128,0),(0x18,-1),(0x14,-1),(0x12c,0),(0x144,-1),(0x11c,clock),(0x134,clock),(0x138,0),(0x13c,0),(0x140,0),(0x160,0)):
  expected[0x588+off:0x58c+off]=w(value)
 expected[0x6e4]=1;expected[0x6f8]=0
 u.mem_write(base,seed);u.mem_write(0x5a3ed8,w(clock));u.mem_write(stack,bytes(128));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);trace=[]
 u.emu_start(0x419981,0x4199c5,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==0x4199c5 and u.reg_read(UC_X86_REG_ESP)==stack
 assert trace==['query','AI'] and bytes(u.mem_read(base,0x1494))==expected
report=dict(result='PASS',cases=512,query_zero_returns=256,query_nonzero_returns=256,scope='Original419981..4199c5 including unchanged40b6c0/40a950/timer/vector callees. Full actor reset footprint, exact navigation arguments, query-before-AI ordering, ignored query return and preserved callback mutations. Only40c2c0 navigation and408ac0 AI supplied. Does not implement navigation selection, AI state selection, scene ownership, outer landing gates or PC/NXDK special branch.')
(root/'artifacts/special-landing-verification.json').write_text(json.dumps(report,indent=2));print(report)
