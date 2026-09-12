"""Compare complete4a70e0 with shared PC/NXDK release-slot ordering."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
b=0x30000000;stack=b+0xe000;stop=b+0xf000
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
u=machine(binary);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_weapon_release_player_slots\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
trace=[];mutation=0

def hook(m,a,n,unused):
 native=m is x
 if a!=(b+0x4000 if native else 0x4cc010):return
 sp=m.reg_read(UC_X86_REG_ESP);token=get(m,sp+(8 if native else 4));trace.append(token)
 slots=b if native else b+0x10e8
 if mutation==1:m.mem_write(slots+96,w(0x12345678))
 if mutation==2:m.mem_write(slots+96,w(0))
 if mutation==3:m.mem_write(slots,w(0xabcdef))
 target=get(m,sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,target)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4a70e0);commands=[];expected=[];releases=0
for i in range(1024):
 slots=[rng.getrandbits(32) if rng.randrange(3) else 0 for _ in range(25)]
 if i<4:slots=[0]*25
 if 4<=i<8:slots=[1]*25
 mutation=i%4;commands.append(w(*slots,mutation))
 u.mem_write(b,bytes([0xa5])*0x2000);u.mem_write(b+0x10e8,w(*slots));before=bytes(u.mem_read(b,0x2000));u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack);trace=[]
 u.emu_start(0x4a70e0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(b,0x2000));assert before[:0x10e8]==after[:0x10e8] and before[0x114c:]==after[0x114c:]
 want=w(0)+after[0x10e8:0x114c]+w(len(trace),*trace)+bytes((25-len(trace))*4);expected.append(want);releases+=len(trace)
 x.mem_write(b,w(*slots));x.mem_write(stack,w(stop,b,b+0x4000,0));x.reg_write(UC_X86_REG_ESP,stack);trace=[]
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,100))+w(len(trace),*trace)+bytes((25-len(trace))*4)
 assert got==want,('NXDK',i)
exe=root/'build/pc/Release/rf_weapon_probe.exe'
assert subprocess.check_output([str(exe),'--player-slots'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=len(commands),releases=releases,original_sha256=digest,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full4a70e0 vs shared PC and compiled NXDK: ordered nonzero releases, clear after callback, future and already-visited slot mutations, empty and full arrays. Original surrounding player bytes unchanged. Resource release4cc010 supplied; no live player/resource pool or XEMU gameplay.')
(root/'artifacts/weapon-player-slots.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
