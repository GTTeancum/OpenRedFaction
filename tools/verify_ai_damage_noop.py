"""Execute original407fb0 mode0 for missing/self damage sources, without stubs."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
handle=0x12340001;other=0x23450002
u.mem_write(0x7394cc+4,w(b,b+0x2000));u.mem_write(b+0x2024,w(0));u.mem_write(b+0x202c,w(other))
u.mem_write(0x7c75cc,w(0));seen=set();boundary=None
effects={0x4091d0,0x409210,0x429ab0,0x4085f0,0x4065d0,0x409050,0x4fa360,0x408ac0,0x4089f0,0x57312d,0x504e40}
def observe(m,address,size,context):
 global boundary
 if address in (0x4270a0,0x40a2f0,0x40a150,0x40a0e0,0x4895d0):seen.add(address)
 if address in effects:boundary=address;m.emu_stop()
u.hook_add(UC_HOOK_CODE,observe)
rng=random.Random(0x407fb0);sources=[0xffffffff,handle,handle^0x10000,0x45670003];counts={s:0 for s in sources}
for i in range(4096):
 obj=bytearray(rng.randbytes(0x1500));cls=bytearray(rng.randbytes(0x800))
 action=[0,1,2,3,8,9,13,17,-1][(i//4)%9]
 for offset,data in ((0x24,w(0)),(0x2c,w(handle)),(0x294,w(b+0x4000)),(0x2a0,w(b)),(0x520,w(action)),
                     (0x7c,w((0,8,0x200000)[(i//36)%3])),(0x810,w(rng.getrandbits(32)))):obj[offset:offset+4]=data
 flags=[0,0x10,0x20000,0x400000,0x4000010,0x420010,0xffffffff][(i//108)%7]
 cls[0x724:0x728]=w(flags);u.mem_write(b,bytes(obj));u.mem_write(b+0x4000,bytes(cls))
 source=sources[i%4];boundary=None;u.mem_write(stack,w(stop,b+0x2a0,source,rng.getrandbits(32),0))
 u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x407fb0,stop,count=100000)
 assert boundary is None and u.reg_read(UC_X86_REG_EIP)==stop,(i,boundary)
 assert bytes(u.mem_read(b,len(obj)))==obj and bytes(u.mem_read(b+0x4000,len(cls)))==cls
 counts[source]+=1
# A valid different source must not be mistaken for the proved no-op family.
u.mem_write(b+0x520,w(0));u.mem_write(b+0x7c,w(0));u.mem_write(b+0x810,w(0));u.mem_write(b+0x4724,w(0x10))
boundary=None;u.mem_write(stack,w(stop,b+0x2a0,other,0x41200000,0));u.reg_write(UC_X86_REG_ESP,stack)
u.emu_start(0x407fb0,stop,count=100000);assert boundary==0x4091d0,boundary
assert {0x4270a0,0x40a2f0,0x40a150,0x40a0e0,0x4895d0}<=seen
report=dict(result='PASS',original_noop_cases=4096,positive_controls=1,source_cases={hex(k):v for k,v in counts.items()},
 scope='Full original407fb0 mode0 with actual gates/object lookup, no callee substitutions. Missing sentinel, absent slot, stale generation and self source return with entire entity/class unchanged and no effects/RNG calls. Empty attached-player list. A valid different source reaches4091d0 and is deliberately not executed further. Forced mode1 and full hostile-source reactions remain separate.')
(root/'artifacts/ai-damage-noop.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
