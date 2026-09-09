"""Verify original entity-loader scalar overlay, excluding creation and I/O."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0x3000;u.mem_map(base,0x10000)
pack=lambda value:struct.pack('<I',value)
r=random.Random(464010);values=[0,1,2,255,256,0xffffffff,0x80000000]+[r.getrandbits(32) for _ in range(93)]
cases=0
for fill in (0,0xa5):
 for index,value in enumerate(values):
  uid=values[(index+1)%len(values)];relation=values[(index+2)%len(values)];byte_source=values[(index+3)%len(values)]
  original=bytes([fill])*0x900;u.mem_write(base,original);u.mem_write(stack,bytes(0x100))
  for offset,v in ((0x8c,0),(0x9c,byte_source),(0xc8,relation),(0xa4,value),(0xd0,uid)):u.mem_write(stack+offset,pack(v))
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,base)
  u.emu_start(0x4647ef,0x46483c,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x46483c
  expected=bytearray(original);expected[0x28]=byte_source&255
  for offset,v in ((0x51c,relation),(0x1f8,value),(0x20,uid),(0x840,0x3f800000)):expected[offset:offset+4]=pack(v)
  assert bytes(u.mem_read(base,0x900))==expected,(fill,index)
  cases+=1
report=dict(result='PASS',cases=cases,scope='Original 4647ef..46483c scalar overlay with supplied loader locals, zero FOV and patterned entity storage. Full 0x900-byte comparison; UID, relationship word, unmodified friendliness and low-byte field verified. File parsing, factory, nonzero FOV and other loader branches excluded.')
(root/'artifacts/entity-loader-fields-verification.json').write_text(json.dumps(report,indent=2));print(report)
