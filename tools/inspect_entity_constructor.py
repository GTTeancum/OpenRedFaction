"""Observe original entity C++ constructor writes without inventing scalar defaults."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,0x10000);u.mem_map(0,0x10000)
rng=random.Random(0x40e380);writes=set();fields=[0x7c,0x1a8,0x200,0x294,0x520,0x554,0x708,0x70c,0x710,0x714,0x718,0x71c,0x7d0,0x810,0x834,0x1380,0x138c]
def observe(uc,access,address,size,value,data):
 if b<=address<b+0x1494:writes.update(range(address-b,address-b+size))
u.hook_add(UC_HOOK_MEM_WRITE,observe)
for case in range(32):
 raw=rng.randbytes(0x1494);u.mem_write(b,raw);u.mem_write(stack,struct.pack('<I',stop))
 u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x40e380,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(b,len(raw)))
 for offset in fields:assert after[offset:offset+4]==raw[offset:offset+4],hex(offset)
 assert all(before==after[i] or i in writes for i,before in enumerate(raw))
ranges=[]
for offset in sorted(writes):
 if ranges and ranges[-1][1]==offset:ranges[-1][1]=offset+1
 else:ranges.append([offset,offset+1])
report=dict(result='PASS',cases=32,original_sha256=digest,preserved_sample_fields=[hex(o) for o in fields],write_ranges_exclusive=[[hex(a),hex(b)] for a,b in ranges],scope='Full original40e380 and real member constructors execute with patterned supplied1494-byte storage. This constructor does not initialize the listed scalar fields. Allocation policy and later486da0/422360 writes are excluded; do not infer the final live values from this observation alone.')
(root/'artifacts/entity-cpp-constructor.json').write_text(json.dumps(report,indent=2));print(report)
