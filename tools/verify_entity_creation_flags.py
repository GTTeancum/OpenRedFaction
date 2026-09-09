"""Compare original entity factory prefix with PC/NXDK flag conversion."""
import hashlib,json,struct,sys,subprocess,re,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def mapped(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);return u
u=mapped(exe);u.mem_map(0,4096);base=0x30000000;u.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
commands=bytearray();expected=bytearray();r=random.Random(422360)
flags_list=list(range(16))+[0xffffffff,0x80000000]+[r.getrandbits(32) for _ in range(46)]
for network in (0,1,2):
 for kind in (0,1,2,3,4,0xffffffff):
  for flags in flags_list:
   u.mem_write(0,pack(0));u.mem_write(0x62f2d0,pack(1));u.mem_write(0x64ecb9,bytes([network]));u.mem_write(0x5cc594,pack(kind))
   u.mem_write(stack,pack(stop,0,0,0,0,0,flags,0xffffffff));u.reg_write(UC_X86_REG_ESP,stack)
   u.emu_start(0x422360,0x422477,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x422477
   actual=bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP)+0x10,4))
   commands.extend(pack(flags,kind));expected.extend(actual)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--creation-flags'],input=commands)
assert raw==expected,'PC flag mismatch'
x=mapped(root/'build/xbox/main.exe');x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_entity_creation_object_flags\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for at in range(0,len(commands),8):
 x.mem_write(stack,pack(stop)+commands[at:at+8]);x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert pack(x.reg_read(UC_X86_REG_EAX))==expected[at//2:at//2+4],('NXDK flag mismatch',at//8)
report=dict(result='PASS',pc_cases=len(commands)//8,nxdk_cases=len(commands)//8,scope='Complete original 422360 entry through 422477, valid class zero and absent player index. Six descriptor kinds, creation flags including upper bits, and network bytes 0/1/2. No allocation, asset initialization or full entity construction.')
(root/'artifacts/entity-creation-flags-verification.json').write_text(json.dumps(report,indent=2));print(report)
