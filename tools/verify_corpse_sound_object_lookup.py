"""Verify corpse459a20 is typed item lookup, not mixer voice lookup."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)//4096*4096);u.mem_write(base,data)
b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
cases=matched=0
for slot,generation,kind,present,stale in itertools.product([0,1,31,1023],[0,1,0x7fff,0xffff],[0,1,2,7,8,0xffffffff],[False,True],[False,True]):
 handle=(generation<<16)|slot
 u.mem_write(0x7394cc,bytes(4096));u.mem_write(b,bytes(0x400))
 u.mem_write(b+0x24,w(kind));u.mem_write(b+0x2c,w(handle^0x10000 if stale else handle))
 if present:u.mem_write(0x7394cc+slot*4,w(b))
 u.mem_write(stack,w(stop,handle));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x459a20,stop,count=1000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=b if present and not stale and kind==1 else 0
 assert u.reg_read(UC_X86_REG_EAX)==expected,(slot,generation,kind,present,stale)
 cases+=1;matched+=bool(expected)
report=dict(result='PASS',cases=cases,matches=matched,original_sha256=digest,scope='Complete unhooked459a20 and40a0e0: generation-bearing world-object handle, exact type1, stale/missing/type mismatch rejection. Type1 is item, corroborated by pickup dispatcher45a3d0 and Dash Faction object/item declarations. No item creation or ownership implemented.')
(root/'artifacts/corpse-sound-object-lookup.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
