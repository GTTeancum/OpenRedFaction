"""Execute complete original controller pending-position commit with real helpers."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000;rng=random.Random(0x46a8f0)
u.mem_write(0x64ecb9,bytes(2));assignments=0
for n in range(2000):
 flags=rng.getrandbits(32);flags=(flags&~0x80000008)|[0,8,0x80000000,0x80000008][n%4]
 blobs=[];poses=[];radii=[]
 for i in range(9):
  blob=bytearray([0xa5]*1024);pose=struct.pack('<3f',*[rng.uniform(-10000,10000) for _ in range(3)]);radius=rng.choice([-1.,0.,.25,16.]);handle=0x12340000+i
  struct.pack_into('<I',blob,0x2c,handle);struct.pack_into('<I',blob,0x7c,rng.getrandbits(32));struct.pack_into('<f',blob,0x180,radius);blob[0xf0:0xfc]=pose
  blobs.append(blob);poses.append(pose);radii.append(radius);u.mem_write(0x7394cc+i*4,struct.pack('<I',base+i*1024))
 struct.pack_into('<I',blobs[0],0x318,flags)
 # Ordered mover and general-object handle lists, with duplicates, absent and stale handles.
 lists=[[rng.choice([0xffffffff,0x56780001]+[0x12340000+i for i in range(1,9)]) for _ in range(rng.randrange(17))] for _ in range(2)]
 for k,offset in enumerate([0x2cc,0x2c0]):
  ptr=base+12000+k*1024;ids=lists[k];struct.pack_into('<3I',blobs[0],offset,len(ids),len(ids),ptr)
  u.mem_write(ptr,struct.pack('<'+'I'*len(ids),*ids) if ids else bytes(4))
 for i,blob in enumerate(blobs):u.mem_write(base+i*1024,bytes(blob))
 u.mem_write(stack,struct.pack('<2I',stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x46a8f0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 touched={0}|{h&0xffff for ids in lists for h in ids if 0x12340001<=h<=0x12340008}
 if not flags&0x80000008:touched=set()
 for i,blob in enumerate(blobs):
  if i in touched:
   assignments+=1;pose=poses[i];p=struct.unpack('<3f',pose);r=radii[i]
   for offset in [0x3c,0xe4,0xf0]:blob[offset:offset+12]=pose
   blob[0x190:0x19c]=struct.pack('<3f',*[v-r for v in p]) if r>0 else pose
   blob[0x19c:0x1a8]=struct.pack('<3f',*[v+r for v in p]) if r>0 else pose
   old,=struct.unpack_from('<I',blob,0x7c);struct.pack_into('<I',blob,0x7c,old|0x4000000)
  if i==0 and touched:struct.pack_into('<I',blob,0x318,flags&~0x80000008)
  assert bytes(u.mem_read(base+i*1024,1024))==blob,(n,i,'object mutation')
report=dict(result='PASS',controllers=2000,objects_checked=18000,objects_committed=assignments,scope='Complete original 46a8f0 with unchanged array access, handle lookup and 48a230 position assignment. Both attachment lists, duplicate/missing/stale handles, all dirty-bit combinations, bounds and whole-object writes. Debug lookup disabled. This proves pending-position commit, not interpolation or a C scene integration.')
(root/'artifacts/group-pose-commit-original.json').write_text(json.dumps(report,indent=2));print(report)
