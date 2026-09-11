"""Original4cd970 cached-room decision with supplied crossing and world lookup."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000;world=b+0x3000;trace=[]
def hook(m,address,size,context):
 if address not in (0x4cd9e0,0x4e1630):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<5I',m.mem_read(sp,20));count=4 if address==0x4cd9e0 else 2
 trace.append((address,*a[1:count+1]));m.reg_write(UC_X86_REG_EAX,crossed if address==0x4cd9e0 else located);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
cases=queries=crossings=0
for old,position,flags,crossed,located in itertools.product((0,0x12345678),(0,0x3f800000,1,0x7fc00000),(0,1,256),(0,1,2,256,257),(0,0x87654321)):
 u.mem_write(b,bytes(12));u.mem_write(b+0x100,w(position,0,0));u.mem_write(stack,w(stop,old,b,b+0x100,flags));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,world);u.reg_write(UC_X86_REG_FPCW,0x27f);trace=[]
 u.emu_start(0x4cd970,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=[];result=old
 if not old:expected.append((0x4e1630,world,b+0x100));result=located
 elif position in (0x3f800000,1):
  expected.append((0x4cd9e0,world,0,b,b+0x100))
  if crossed&255:expected.append((0x4e1630,world,b+0x100));result=located
 assert trace==expected and u.reg_read(UC_X86_REG_EAX)==result,(old,position,flags,crossed,located,trace,expected)
 queries+=sum(row[0]==0x4e1630 for row in trace);crossings+=sum(row[0]==0x4cd9e0 for row in trace);cases+=1
report=dict(result='PASS',cases=cases,world_queries=queries,crossing_queries=crossings,original_sha256=digest,scope='Complete unmodified4cd970 with real squared-distance4faf00. Missing room always queries4e1630; cached room moves only after positive squared displacement and nonzero low-byte4cd9e0 result. Fourth input ignored; helper receives null cached tree. Zero/NaN displacement retain old room, smallest positive float moves under x87. Crossing/locator supplied; no actual world geometry or shared implementation.')
(root/'artifacts/room-tracking-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
