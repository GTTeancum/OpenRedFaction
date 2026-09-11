"""Original428740 first-person early return with real player/view predicates."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;player=base+0x3000;view=base+0x5000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,65536);w=lambda *v:struct.pack('<'+'I'*len(v),*v)
rng=random.Random(0x428740);reached=[]
def boundary(m,a,size,context):
    if a==0x4fa3f0:reached.append(a);m.emu_stop()
u.hook_add(UC_HOOK_CODE,boundary)
for case in range(2048):
    actor=bytearray(rng.randbytes(0x1500));owner=bytearray(rng.randbytes(0x1204));camera=bytearray(rng.randbytes(0x80))
    flags=rng.getrandbits(32)|8;actor[0x7c:0x80]=w(flags);actor[0x1430:0x1434]=w(player)
    owner[0xc4:0xc8]=w(view);camera[8:12]=w(0)
    u.mem_write(base,bytes(actor));u.mem_write(player,bytes(owner));u.mem_write(view,bytes(camera))
    u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);reached.clear()
    u.emu_start(0x428740,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and not reached
    assert bytes(u.mem_read(base,len(actor)))==actor
    assert bytes(u.mem_read(player,len(owner)))==owner and bytes(u.mem_read(view,len(camera)))==camera
# Distinguish mode0 from other modes, missing association and missing player bit.
for mode,pointer,flags in ((1,player,8),(0x100,player,8),(0xffffffff,player,8),(0,0,8),(0,player,0)):
    u.mem_write(base+0x7c,w(flags));u.mem_write(base+0x1430,w(pointer));u.mem_write(view+8,w(mode))
    reached.clear();u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x428740,stop,count=10000);assert reached==[0x4fa3f0]
report=dict(result='PASS',cases=2048,positive_controls=5,original_sha256=digest,
 scope='Complete original428740 early return with unchanged42a8e0,4895d0,40d740. Prepared real-layout actor/player/view links and mode0; whole owners unchanged and no timer/RNG/effects reached. Nonzero modes and absent associations/player bit reach the first cooldown query. No full camera lifecycle.')
(root/'artifacts/player-pain-gate.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
