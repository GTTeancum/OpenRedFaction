"""Controller mover-bound gathering against original lookup/copy/zero loop."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(b,(len(im)+4095)//4096*4096);m.mem_write(b,im);m.mem_map(base,65536);return m
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_group_wake_bounds_collect\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.hook_add(UC_HOOK_CODE,lambda m,a,s,c:m.emu_stop(),begin=0x46ae65,end=0x46ae65)
rng=random.Random(0x46ad19);commands=bytearray();expected=bytearray();zero=0
for n in range(1024):
 count=n%35;handles=[rng.choice([0x10000,0x20001,0x30002,0x40003,0x90000,0xffffffff,1024]) for _ in range(34)]
 movers=[w(rng.choice([9,9,8,0]))+struct.pack('<6f',*(rng.uniform(-20,20) for _ in range(6))) for i in range(4)]
 commands.extend(w(count,*handles)+b''.join(movers));u.mem_write(base,bytes(4096));u.mem_write(base+0x2cc,w(count,count,base+0x800));u.mem_write(base+0x800,w(*handles))
 u.mem_write(0x7394cc,bytes(4096));x.mem_write(base,bytes(12288));x.mem_write(base+0x7000,w(*handles))
 for i,mover in enumerate(movers):
  handle=((i+1)<<16)|i;ptr=base+0x1000+i*0x1000;u.mem_write(ptr,bytes(4096));u.mem_write(ptr+0x24,mover[:4]);u.mem_write(ptr+0x2c,w(handle));u.mem_write(ptr+0x190,mover[4:]);u.mem_write(0x7394cc+i*4,w(ptr))
  wrapper=base+0x4000+i*16;pose=base+0x5000+i*256;x.mem_write(base+i*8,w(wrapper,handle));x.mem_write(wrapper,mover[:4]+w(handle,pose));x.mem_write(pose,bytes(236));x.mem_write(pose+212,mover[4:])
 u.mem_write(stack,bytes(1024));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base);u.reg_write(UC_X86_REG_ECX,base+0x2cc)
 u.emu_start(0x46ad19,0x46af8d,count=100000);assert u.reg_read(UC_X86_REG_EIP)==(0x46ae65 if count else 0x46af8d)
 length=min(count,32);values=bytes(u.mem_read(stack+0x34,length*24));zero+=sum(values[i:i+24]==bytes(24) for i in range(0,len(values),24))
 want=w(0,length)+values+bytes([0xa5])*(768-len(values));expected.extend(want)
 x.mem_write(base+0x8000,bytes([0xa5])*772);x.mem_write(stack,w(stop,base,base+0x7000,count,base+0x8004,base+0x8000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x8000,772));assert got==want,('NXDK',n)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-wake-bounds'],input=commands);assert actual==expected
report=dict(result='PASS',cases=1024,zero_bound_slots=zero,original_sha256=digest,scope='Original 46ad19..46ae65 gather prefix, constructor, array, type9/full-generation lookup and copy/zero helpers unchanged. Exact PC/NXDK first32 ordered bounds including duplicates, missing/stale/wrong-kind handles and empty arrays. Unused output entries preserved by port contract. Live mover registration/pose refresh excluded.')
(root/'artifacts/group-wake-bounds-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
