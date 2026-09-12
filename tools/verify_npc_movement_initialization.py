"""Original embedded AI clock initialization and attachment store, scoped explicitly."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
resolved=base
# Only registration lookup is supplied in the prefix; timer callees run unchanged.
def hook(cpu,address,size,data):
 if address==0x426fc0:
  sp=cpu.reg_read(UC_X86_REG_ESP);assert r(sp+4)==0x12340005
  cpu.reg_write(UC_X86_REG_EAX,resolved);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x402c20)
for case in range(256):
 seed=rng.randbytes(0x1494);u.mem_write(base,seed);resolved=base if case%2 else 0
 u.mem_write(stack,w(stop,base+0x2a0,0x12340005,case%2));u.reg_write(UC_X86_REG_ESP,stack)
 finish=0x402d85 if resolved else stop
 u.emu_start(0x402c20,finish,count=100000);assert u.reg_read(UC_X86_REG_EIP)==finish
 if resolved:
  assert r(base+0x7b0)==r(base+0x7b4)==r(base+0x7b8)==0
  assert r(base+0x75c)==struct.unpack_from('<I',seed,0x75c)[0]
  assert u.reg_read(UC_X86_REG_EBX)==0 and u.reg_read(UC_X86_REG_EBP)==0xffffffff
  # Later straight-line attachment initialization; intermediate AI services excluded.
  before=bytes(u.mem_read(base,0x1494));u.emu_start(0x402e90,0x402eac,count=100)
  want=bytearray(before)
  for offset,value in ((0x75c,0xffffffff),(0x750,0xffffffff),(0x760,0x43480000)):want[offset:offset+4]=w(value)
  assert bytes(u.mem_read(base,0x1494))==want
 else:
  want=bytearray(seed);want[0x2a0:0x2a4]=w(0);assert bytes(u.mem_read(base,0x1494))==want
# Actual factory argument preparation proves the embedded-owner offset.
u.mem_write(base+0x2c,w(0x12340005));u.mem_write(stack+0x3c,w(0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base)
u.emu_start(0x422c9f,0x422caf,count=100);sp=u.reg_read(UC_X86_REG_ESP)
assert bytes(u.mem_read(sp,12))==w(base+0x2a0,0x12340005,0)
assert bytes(u.mem_read(0x422caf,1))==b'\xe8' and struct.unpack('<i',u.mem_read(0x422cb0,4))[0]+0x422cb4==0x402c20
report=dict(result='PASS',cases=256,registered_clock_prefixes=128,unresolved_no_initialization=128,attachment_store_blocks=128,factory_argument_blocks=1,scope='Original402c20 entry through402d85 with only426fc0 lookup supplied; original timer callees unchanged. Separate402e90..402eac attachment block uses prefix-established ESI/EBX/EBP; intervening AI services and complete factory excluded. Actual422c9f..422caf argument preparation proves actor+2a0 owner mapping. No claim of complete gameplay initialization or live NPC scheduling.')
(root/'artifacts/npc-movement-initialization-verification.json').write_text(json.dumps(report,indent=2));print(report)
