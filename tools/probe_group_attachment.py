"""Original controller mover-attachment loop with real UID lookup/list helpers."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=root/'Installed_Game/RF.exe';assert hashlib.sha256(p.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(p)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);base=0x30000000;u.mem_map(base,0x400000)
controller=base;refs=base+4096;handles=base+8192;key=base+12288;keylist=base+13000;objects=base+65536;stack=base+0x300000
f32=lambda f:struct.unpack('<f',struct.pack('<f',f))[0]
factor,=struct.unpack_from('<f',im,0x189510)
levels=json.loads((root/'artifacts/moving-groups.json').read_text())['results'];movers=json.loads((root/'artifacts/movers.json').read_text())['results'];results=[];synthetic=[]
def initialize(items):
 for i,(uid,kind,flags) in enumerate(items):
  ptr=objects+i*1024;u.mem_write(ptr,bytes(1024));u.mem_write(ptr+0x10,struct.pack('<I',ptr+1024 if i+1<len(items) else 0x73d880));u.mem_write(ptr+0x20,struct.pack('<II',uid&0xffffffff,kind));u.mem_write(ptr+0x2c,struct.pack('<II',0x12340000+i,0xffffffff));u.mem_write(ptr+0x7c,struct.pack('<I',flags))
 u.mem_write(0x73d890,struct.pack('<I',objects if items else 0x73d880))
def attach(ids,flags,handle,global_mode=0):
 u.mem_write(controller,bytes(1024));u.mem_write(refs,struct.pack('<'+'I'*len(ids),*ids) if ids else bytes(4));u.mem_write(handles,bytes(max(4,4*len(ids))));u.mem_write(controller+0x2b4,struct.pack('<3I',len(ids),len(ids),refs));u.mem_write(controller+0x2cc,struct.pack('<3I',0,max(1,len(ids)),handles));u.mem_write(controller+0x2c,struct.pack('<I',handle));u.mem_write(controller+0x318,struct.pack('<I',flags));u.mem_write(controller+0x29c,struct.pack('<3I',1,1,keylist));u.mem_write(keylist,struct.pack('<I',key));u.mem_write(key,bytes(100));u.mem_write(key+0x54,struct.pack('<f',1));u.mem_write(0x64e97c,bytes([global_mode]));u.reg_write(UC_X86_REG_EDI,controller);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x46b6e8,0x46b79c,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==0x46b79c
 n,=struct.unpack('<I',u.mem_read(controller+0x2b4,4));h,=struct.unpack('<I',u.mem_read(controller+0x2cc,4));rotation,=struct.unpack('<f',u.mem_read(key+0x54,4))
 return list(struct.unpack('<'+'I'*n,u.mem_read(refs,n*4))),list(struct.unpack('<'+'I'*h,u.mem_read(handles,h*4))),rotation
for level in levels:
 ml=next(l for l in movers if l['file']==level['file'] and l['archive']==level['archive']);items=[(m['uid'],9,0) for m in ml['records']];initialize(items);lookup={uid&0xffffffff:i for i,(uid,_,_) in enumerate(items)};parents=[0xffffffff]*len(items);expected_flags=[0]*len(items);memberships=0
 for index,g in enumerate(level['records']):
  f=g['flags'];flags=(0x80000100 if f[2] else 0x80002000)|(2 if f[0] else 0)|(4 if f[1] else 0)
  for j in range(3,6):
   if f[j]:flags|=1<<(j+7)
  handle=0x23450000+index;ids=g['ids2'];remaining,actual,rotation=attach(ids,flags,handle)
  assert remaining==ids and actual==[0x12340000+lookup[uid] for uid in ids]
  expected_rotation=1.
  for uid in ids:
   i=lookup[uid];parents[i]=handle
   if flags&0x1000:expected_flags[i]|=0x40000
   if flags&4 and not flags&0x2100:expected_rotation=f32(expected_rotation*factor)
  assert rotation==expected_rotation;memberships+=len(ids)
 for i in range(len(items)):
  assert struct.unpack('<I',u.mem_read(objects+i*1024+0x30,4))[0]==parents[i]
  assert struct.unpack('<I',u.mem_read(objects+i*1024+0x7c,4))[0]==expected_flags[i]
 results.append(dict(file=level['file'],groups=len(level['records']),memberships=memberships))
for flags in [0,4,0x104,0x2004,0x1004]:
 for mode in [0,1]:
  initialize([(10,9,0),(20,8,0),(30,9,0)]);remaining,actual,rotation=attach([10,999,20,30,10],flags,55,mode)
  assert remaining==[10,30,10] and actual==[0x12340000,0x12340002,0x12340000]
  want=1.
  if flags&4 and not flags&0x2100 and not mode:
   for _ in range(3):want=f32(want*factor)
  assert rotation==want;synthetic.append(dict(flags=flags,global_mode=mode,remaining=remaining,rotation=rotation))
report=dict(result='PASS',levels=len(results),memberships=sum(l['memberships'] for l in results),synthetic_cases=len(synthetic),rotation_multiplier=factor,scope='Original 46b6e8..46b79c with unchanged 48a4a0 UID lookup, append, removal and gate helpers. All installed mover links, cumulative parent/flags in group order; synthetic missing/wrong-type/duplicate references and rotation gate. Preallocated arrays avoid allocator branch. Not full controller attachment pass, first ID-list handling, saved-state branch or playback.',results=results,synthetic=synthetic)
(root/'artifacts/group-attachment-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k not in ('results','synthetic')})
