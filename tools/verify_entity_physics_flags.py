"""Compare original class-dependent physics flags with PC/NXDK."""
import hashlib,json,struct,sys,subprocess,re,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def mapped(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);return u
u=mapped(exe);u.mem_map(0,4096);base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
commands=bytearray();expected=bytearray();r=random.Random(42268)
flags_list=[sum(bit for j,bit in enumerate((0x40000,0x4000,0x400000,0x1000,0x200)) if mask&(1<<j)) for mask in range(32)]+[r.getrandbits(32) for _ in range(32)]
for network in (0,1,2):
 for kind in (0,4,0xffffffff):
  for second in (0,2,0xfffffffd,0xffffffff):
   for flags in flags_list:
    for creation in (0,1,0xfffffffe,0xffffffff):
     u.mem_write(base,bytes(0x800));u.mem_write(base+0x724,pack(flags,second));u.mem_write(base+0x1b4,pack(kind));u.mem_write(0x64ecb9,bytes([network]))
     u.mem_write(stack,bytes(0x140));u.mem_write(stack+0x2c,pack(creation&1,0,creation&1));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,base)
     u.emu_start(0x42268b,0x42270e,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x42270e
     commands.extend(pack(creation,flags,second,kind,network));expected.extend(bytes(u.mem_read(stack+0x124,4)))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--physics-flags'],input=commands)
assert raw==expected,'PC physics flag mismatch'
x=mapped(root/'build/xbox/main.exe');x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_entity_creation_physics_flags\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for at in range(0,len(commands),20):
 x.mem_write(stack,pack(stop)+commands[at:at+20]);x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert pack(x.reg_read(UC_X86_REG_EAX))==expected[at//5:at//5+4],('NXDK physics flag mismatch',at//20)
report=dict(result='PASS',pc_cases=len(commands)//20,nxdk_cases=len(commands)//20,scope='Original 42268b..42270e with seeded class fields and already-normalized player locals from creation bit zero. Exhaustive relevant class-mask combinations plus random upper bits; kind 0/4/-1, secondary flags and network bytes 0/1/2. No intercepted calls. Does not parse entity.tbl or construct complete entities.')
(root/'artifacts/entity-physics-flags-verification.json').write_text(json.dumps(report,indent=2));print(report)
