"""Complete original Set_Friendliness action and UnHide request methods."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000;array=base+0x1000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def call(address):
 u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(address,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
cases=0
for value in (0,1,2,3,0xffffffff,0x80000000):
 for count in (0,1,6,12):
  u.mem_write(0x7394cc,bytes(4096));u.mem_write(base,bytes(0x2c0));objects=[]
  for i in range(6):
   obj=base+0x2000+i*0x800;raw=bytearray([0xa5]*0x600)
   struct.pack_into('<I',raw,0x24,i);struct.pack_into('<I',raw,0x2c,0x12340000+i)
   u.mem_write(obj,bytes(raw));u.mem_write(0x7394cc+4*i,pack(obj));objects.append((obj,raw))
  links=[0x12340000,0x12340001,0x12340002,0xffffffff,0x99990000,0x12340400,0x12340003,0x12340004,0x12340005,0x12340000,0x12340009,0x12340001][:count]
  u.mem_write(array,pack(*links));u.mem_write(base+0x29c,pack(count,count,array));u.mem_write(base+0x2b8,pack(value))
  call(0x4bc280)
  for i,(obj,want) in enumerate(objects):
   if 0x12340000+i in links:
    struct.pack_into('<I',want,0x1f8,value)
    if i==0:struct.pack_into('<I',want,0x560,0xffffffff)
   assert bytes(u.mem_read(obj,0x600))==want,(value,count,i)
  cases+=1
request_cases=0
for a,offset in ((0x4bcdd0,0x2bc),(0x4bcde0,0x2bd)):
 for fill in (0,1,0xa5,0xff):
  want=bytearray([fill]*0x2c0);u.mem_write(base,bytes(want));call(a);want[offset]=1
  assert bytes(u.mem_read(base,len(want)))==want;request_cases+=1
for kind in (6,30):
 want=bytearray([0xa5]*0x2c0);struct.pack_into('<I',want,0x290,kind);u.mem_write(base,bytes(want));call(0x4b9f80)
 assert bytes(u.mem_read(base,len(want)))==want
report=dict(result='PASS',friendliness_cases=cases,unhide_request_cases=request_cases,off_noop_cases=2,scope='Complete original 4bc280/489f70/4895f0, real array and handle lookup, UnHide on/off request methods, and common off method for types 6/30. Synthetic registered objects, full object-byte comparisons. No shared action implementation or deferred UnHide processing.')
(root/'artifacts/door-event-actions-verification.json').write_text(json.dumps(report,indent=2));print(report)
