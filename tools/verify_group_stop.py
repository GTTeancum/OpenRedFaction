"""Original controller stop46b5b0 vs PC/NXDK, no hooked callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_group_motion_stop\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c,0x2f4,0x308]
rng=random.Random(0x46b5b0);commands=bytearray();expected=bytearray();handle=0x10000
for case in range(1024):
 raw=bytearray(rng.randbytes(1024));raw[0x24:0x28]=w(8);raw[0x2c:0x30]=w(handle)
 flags=(rng.getrandbits(32)&~0x60)|((case%4)<<5);raw[0x318:0x31c]=w(flags)
 data=b''.join(raw[o:o+4] for o in offsets);u.mem_write(base,bytes(raw));u.mem_write(0x7394cc,w(base))
 u.mem_write(stack,w(stop,handle));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x46b5b0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 after=bytes(u.mem_read(base,1024));result=b''.join(after[o:o+4] for o in offsets)
 allowed=(0x318,0x308,0x2f4) if flags&0x40 else (0x2fc,0x2f4)
 for o in allowed:raw[o:o+4]=after[o:o+4]
 assert after==raw
 commands.extend(data);expected.extend(w(0)+result)
 x.mem_write(base,data);x.mem_write(stack,w(stop,base,base+24,base+28));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,32))==result,case
# Missing, stale-generation and wrong-type lookups leave the whole object untouched.
for kind,registered in ((8,0),(8,base),(6,base)):
 raw=bytearray(rng.randbytes(1024));raw[0x24:0x28]=w(kind);raw[0x2c:0x30]=w(handle if kind==6 else 0x20000)
 u.mem_write(base,bytes(raw));u.mem_write(0x7394cc,w(registered));u.mem_write(stack,w(stop,handle));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x46b5b0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop and bytes(u.mem_read(base,1024))==raw
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-stop'],input=commands);assert actual==expected
report=dict(result='PASS',cases=1024,lookup_rejections=3,original_sha256=digest,scope='Complete46b5b0, actual46afa0/40a0e0 typed generation lookup and no-op46b610, no hooks. Whole original controller unchanged except verified fields; shared PC/NXDK motion/speed/raw308 exact. Synthetic arbitrary bit patterns and flag20/40 combinations. Campaign stop integration and rotation runtime ownership remain open.')
(root/'artifacts/group-stop.json').write_text(json.dumps(report,indent=2));print(report)
