"""Exercise concrete corpse/body pool ownership in PC and compiled NXDK code."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
print(subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-owners'],text=True).strip())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;u.mem_map(b,0x20000);seed=b+0x6000;source=b+0x6100;out=b+0x6200;stack=b+0x1e000;stop=b+0x1f000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
init=sym('rf_corpse_owners_init');acquire=sym('rf_corpse_owners_acquire');recycle=sym('rf_corpse_owners_recycle');malloc=sym('malloc');free=sym('free')
base_bytes=19224;record_bytes=636;live=set();heap_calls=[];fail=False

def heap(cpu,address,size,data):
 if address not in (malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4);heap_calls.append((address,arg))
 if address==malloc:
  assert arg==24
  pointer=0 if fail else next(b+0x8000+i*256 for i in range(30) if b+0x8000+i*256 not in live)
  if pointer:live.add(pointer);cpu.mem_write(pointer,bytes([0xa5])*24)
  cpu.reg_write(UC_X86_REG_EAX,pointer)
 else:
  assert arg in live;live.remove(arg);cpu.mem_write(arg,bytes([0xdd])*24)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,heap)
def call(address,*args):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(address,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
material=struct.unpack('<3I',f(.25,.5,2));sphere=f(1,2,3,1,-1)+w(7)
u.mem_write(seed,f(10,3,0,0,0,1,0,0,0,1,0,0,0,1,1)+w(source,1,0x33));u.mem_write(source,sphere)
u.mem_write(b,bytes([0xa5])*base_bytes);assert call(init,b,base_bytes-1)==0xfffffffc and bytes(u.mem_read(b,base_bytes))==bytes([0xa5])*base_bytes
assert call(init,b,base_bytes+720)==0
for cycle in range(64):
 for i in range(30):
  assert call(acquire,b,seed,*material,out)==0 and read(out)==i
  body=b+136+i*record_bytes+276;assert bytes(u.mem_read(read(body+308),24))==sphere
  u.mem_write(b+136+i*record_bytes+128,w(cycle*30+i))
 u.mem_write(out,w(99));before=len(heap_calls)
 assert call(acquire,b,seed,*material,out)==0xfffffffd and read(out)==99 and len(heap_calls)==before
 assert read(b+base_bytes-8)==base_bytes+720 and len(live)==30
 for i in reversed(range(30)):
  assert call(recycle,b,i)==0 and read(b+136+i*record_bytes+128)==cycle*30+i
  assert call(recycle,b,i)==0xfffffffc
 assert read(b+base_bytes-8)==base_bytes and not live and read(b+124)==0
# Failed body acquisition must restore availability, preserve the output, and leak nothing.
u.mem_write(b+base_bytes-4,w(base_bytes+23));u.mem_write(out,w(99));before=len(heap_calls)
assert call(acquire,b,seed,*material,out)==0xfffffffc and read(out)==99 and len(heap_calls)==before
u.mem_write(b+base_bytes-4,w(base_bytes+24));fail=True
assert call(acquire,b,seed,*material,out)==0xfffffffc and read(out)==99 and not live
fail=False;assert call(acquire,b,seed,*material,out)==0 and read(out)==0
u.mem_write(source,bytes([0xa5])*24);body=b+136+276;assert bytes(u.mem_read(read(body+308),24))==sphere
assert call(recycle,b,0)==0 and read(b+base_bytes-8)==base_bytes and not live
report=dict(result='PASS',cycles=64,successful_acquisitions=1921,capacity=30,xbox_fixed_bytes=base_bytes,xbox_peak_bytes=base_bytes+720,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Real shared pool/body ownership, full-pool rejection, LIFO recycling, retained corpse payload, independent spheres, exact/short budgets and injected NXDK heap failure. Original allocation/body parity established in separate verifiers; no registration/model/scene binding or native XEMU run.')
(root/'artifacts/corpse-owners-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

