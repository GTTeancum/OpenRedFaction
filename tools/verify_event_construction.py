"""Original door/gravity event constructors against patterned storage; no registration."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);u.mem_map(0,4096)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
results=[]
for kind,address,size,vtable in [(6,0x4be510,0x2bc,0x58995c),(30,0x4be8a0,0x2bc,0x589a7c),(44,0x4beb20,0x2bc,0x589b3c),(50,0x4becb0,0x2c0,0x589bec)]:
 for fill in (0,0xa5):
  u.mem_write(base,bytes([fill])*size);u.mem_write(stack,struct.pack('<I',stop));u.mem_write(0,struct.pack('<I',0xffffffff))
  u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(address,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==base
  raw=bytes(u.mem_read(base,size));assert struct.unpack_from('<I',raw)[0]==vtable
  assert raw[0x298:0x29c]==b'\xff'*4 and raw[0x29c:0x2a8]==bytes(12)
  if kind==50:assert raw[0x2b8:0x2bc]==b'\xff'*4
  if kind==44:assert raw[0x2b8:0x2bc]==bytes([fill])*4
  for offset in (0x290,0x294,0x2a8,0x2ac,0x2b0):assert raw[offset:offset+4]==bytes([fill])*4
  assert raw[0x2b4]==fill
  results.append(dict(type=kind,fill=fill,constructor=hex(address),bytes=size,vtable=hex(vtable),on=hex(struct.unpack_from('<I',b,vtable-0x400000+4)[0]),off=hex(struct.unpack_from('<I',b,vtable-0x400000+8)[0])))
report=dict(result='PASS',cases=len(results),scope='Complete original derived constructors and base/timer/empty-array constructors execute unchanged on zero and A5 storage. No allocator, generic object factory, loader overlays or registration executed.',results=results)
(root/'artifacts/event-construction-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
