"""Execute original optional numeric packing with supplied parser results."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EAX

def words(*v):return struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_particle_flags_pack\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
labels=[0x5a0058,0x5a0068,0x5a0078,0x5a0088]
current=0;seen=[]
def parser(m,address,size,data):
 global current
 if address not in (0x5125c0,0x512750):return
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address==0x5125c0:
  label=struct.unpack('<I',m.mem_read(sp+4,4))[0];current=labels.index(label);seen.append(current)
  value=1 if present&(1<<current) else 0;pop=8
 else:value=values[current];pop=4
 m.reg_write(UC_X86_REG_EAX,value&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,parser)
rng=random.Random(497590)
fixtures=[]
for present in range(16):
 for value in (0,1,15,16,17,-1,-16,-17,0x7fffffff,-0x80000000):
  for initial in (0,0xa5a5a5a5,0xffffffff):fixtures.append((present,[value]*4,initial,initial))
for _ in range(512):fixtures.append((rng.getrandbits(32),[rng.getrandbits(32) for _ in range(4)],rng.getrandbits(32),rng.getrandbits(32)))
commands=bytearray();expected=bytearray()
for present,values,particle,secondary in fixtures:
 emitter=0x1234;seen.clear();u.mem_write(base,bytes([0xa5])*128);u.mem_write(base+0x74,words(particle,secondary))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.emu_start(0x497afe,0x497ba2,count=10000)
 assert seen==list(range(4)) and u.reg_read(UC_X86_REG_EIP)==0x497ba2 and u.reg_read(UC_X86_REG_ESP)==stack
 original=words(emitter)+bytes(u.mem_read(base+0x74,8));expected.extend(original)
 command=words(emitter,particle,secondary,present,*values);commands.extend(command);x.mem_write(base,command)
 x.mem_write(stack,words(stop,base,present,base+16));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,12))==original,(present,values,original.hex())
probe=root/'build/pc/Release/rf_effect_probe.exe'
assert subprocess.check_output([str(probe),'--particle-pack'],input=commands)==expected
report=dict(result='PASS',cases=len(fixtures),original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original 497afe..497ba2 with optional-label and integer parser seams supplied, compared with PC/NXDK. All presence combinations, signed wrapping and existing-bit preservation. Full table parser and simulation excluded.')
(root/'artifacts/particle-packing-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
