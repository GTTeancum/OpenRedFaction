"""Original emitter phase duration, including actual CRT random advancement."""
import hashlib,json,re,struct,random,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
# Explicit 53-bit, nearest, masked x87 environment. Unicorn defaults to a zero
# control word (24-bit/unmasked); do not mistake that for a runtime measurement.
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_particle_cycle_duration\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
thread=base+0x1000;draws=0
def thread_data(m,a,s,d):
 global draws
 if a!=0x577eef:return
 draws+=1;sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0];m.reg_write(UC_X86_REG_EAX,thread);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,thread_data);rng=random.Random(49660);fixtures=[]
for enabled in (0,1,2,255,256,257,0xffffffff):
 for cycle in ((1,0,1,0),(0,0,0,0),(0.1,0,0.1,0),(0.2,1,0.5,2),(-1,-2,-3,-4)):
  for seed in (0,1,0xffffffff,0x80000000):fixtures.append((cycle,enabled,seed))
for i in range(1000):fixtures.append((tuple(rng.randint(-1000,10000)/256 for _ in range(4)),rng.getrandbits(32),rng.getrandbits(32)))
commands=bytearray();expected=bytearray()
for cycle,enabled,seed in fixtures:
 draws=0;u.mem_write(base,bytes(0x144));u.mem_write(base+0x128,struct.pack('<4f',*cycle));u.mem_write(base+0x140,bytes([enabled&255]));u.mem_write(thread+0x14,struct.pack('<I',seed));u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x496f60,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and draws==1
 result=bytes(4)+bytes(u.mem_read(thread+0x14,4))+bytes(u.mem_read(base+0x138,4));expected.extend(result)
 command=struct.pack('<4fII',*cycle,enabled,seed);commands.extend(command);x.mem_write(base,command);x.mem_write(base+0x100,bytes([0xa5])*4);x.mem_write(stack,struct.pack('<5I',stop,base,enabled,base+20,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(4)+bytes(x.mem_read(base+20,4))+bytes(x.mem_read(base+0x100,4))==result,(cycle,enabled,seed)
probe=root/'build/pc/Release/rf_effect_probe.exe';assert subprocess.check_output([str(probe),'--particle-duration'],input=commands)==expected
report=dict(result='PASS',cases=len(fixtures),original_sha256=sha,scope='Original 496f60, 504db0 and 57312d executed unchanged, CRT thread-storage pointer supplied. Exact PC/NXDK duration and RNG state, including zero variance, clamp and low-byte selection. Emitter creation and live emission excluded.')
(root/'artifacts/particle-duration-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
