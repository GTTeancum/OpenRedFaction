"""Verify original vertex adjacency insertion before retained topology binding."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u

o=machine(exe);rng=random.Random(0x4ce1e0);sequences=2048;calls=0;duplicates=0
for i in range(sequences):
 expected=[];capacity=64;o.mem_write(B,w(0,capacity,B+256));o.mem_write(B+256,bytes([165])*256)
 for j in range(1+i%64):
  face=0x12340000+4*rng.randrange(16);calls+=1;duplicates+=face in expected
  if face not in expected:expected.append(face)
  o.mem_write(STACK,w(STOP,face));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,B)
  o.emu_start(0x4ce1e0,STOP,count=10000);assert o.reg_read(UC_X86_REG_EIP)==STOP
  count,cap,ptr=struct.unpack('<3I',o.mem_read(B,12));assert (count,cap,ptr)==(len(expected),capacity,B+256)
  assert bytes(o.mem_read(ptr,count*4))==w(*expected)
  assert bytes(o.mem_read(ptr+count*4,(capacity-count)*4))==bytes([165])*((capacity-count)*4)
report=dict(result='PASS',original_sequences=sequences,insertions=calls,duplicate_insertions=duplicates,original_sha256=sha,scope='Full original4ce1e0 find-or-append with actual4ce360 and45ec40, preallocated capacity avoids allocator; checks first-occurrence order and duplicate suppression after every insertion. Original4e0140 calls this helper for each corner vertex+0x20. Loader rejection/removal and retained topology implementation excluded.')
(root/'artifacts/lightmap-adjacency-order.json').write_text(json.dumps(report,indent=2));print(report)
