"""Compare original particle flag-string spans with PC/NXDK decoding."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX
def words(*v):return struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_particle_flags_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
emit=['immediate','continuous','dirdepend','dont_move_with_parent','accel_with_parent']
parts=['glow','clr_change','gravity','collide','collide_liquid','collide_and_die','wind','accelerate','loop','explode','random_orient','vel_stretch','no_z_check','damages','hold_last_frame','fire_damage']
fixtures=[('',''),(' '.join(emit),' '.join(parts))]
for group,names in enumerate((emit,parts)):
 for name in names:
  for value in (name,name.upper(),name[:-1],'prefix'+name+'suffix'):
   fixtures.append((value,'') if group==0 else ('',value))
rng=random.Random(497590)
for _ in range(512):fixtures.append((' '.join(n for n in emit if rng.randrange(2)),' '.join(n for n in parts if rng.randrange(2))))
commands=bytearray();expected=bytearray()
for emitter,particle in fixtures:
 strings=[s.encode().ljust(512,b'\0') for s in (emitter,particle)];commands.extend(b''.join(strings))
 u.mem_write(base,bytes([0xa5])*128)
 for text,start,end in ((emitter,0x4976a4,0x49771b),(particle,0x497946,0x497afe)):
  u.mem_write(base+0x2000,text.encode()+b'\0');u.mem_write(stack+0x14,words(len(text),base+0x2000))
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,0 if start==0x4976a4 else 16)
  u.emu_start(start,end,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==end and u.reg_read(UC_X86_REG_ESP)==stack
 original=words(struct.unpack('<H',u.mem_read(base+0x34,2))[0])+bytes(u.mem_read(base+0x74,8))
 expected.extend(original)
 x.mem_write(base+0x2000,b''.join(strings));x.mem_write(base,bytes([0xa5])*12)
 x.mem_write(stack,words(stop,base+0x2000,base+0x2200,base));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,12))==original,(emitter,particle,original.hex(),bytes(x.mem_read(base,12)).hex())
probe=root/'build/pc/Release/rf_effect_probe.exe'
assert subprocess.check_output([str(probe),'--particle-flags'],input=commands)==expected
report=dict(result='PASS',cases=len(fixtures),original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original 4976a4..49771b and 497946..497afe plus unchanged string search; PC/NXDK three output masks match. Scalar/boolean/numeric packing, table parsing and particle simulation excluded.')
(root/'artifacts/particle-flags-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
