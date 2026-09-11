"""Original generic factory and class object-flag additions for NPC ownership."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);rng=random.Random(0x486f0f)
for n in range(2048):
 incoming=rng.getrandbits(32);class_flags=rng.getrandbits(32)
 u.mem_write(stack+0x3c,pack(incoming));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base)
 u.emu_start(0x486f0f,0x486f63,count=100)
 assert u.reg_read(UC_X86_REG_EIP)==0x486f63
 assert u.reg_read(UC_X86_REG_ESP)==stack-4
 want=incoming|0x06000000|(0x8000 if incoming&0x4000 else 0)
 assert bytes(u.mem_read(base+0x7c,4))==pack(want)
 assert bytes(u.mem_read(base+0x200,8))==pack(0xffffffff,0xffffffff)
 u.reg_write(UC_X86_REG_EDI,base+0x4000);u.mem_write(base+0x4728,pack(class_flags))
 u.emu_start(0x422b9a,0x422baa,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x422baa
 if not class_flags&1:want|=0x20000
 assert bytes(u.mem_read(base+0x7c,4))==pack(want)
report=dict(result='PASS',cases=2048,original_sha256=sha,scope='Unmodified generic486f0f..486f63 then prepared class422b9a..422baa. Confirms06000000, conditional8000 and class728 bit0 fallback20000 used by retained NPC owners. Entity input conversion and vitals have separate PC/NXDK verifiers. This harness does not run intervening factory code or claim complete creation semantics.')
(root/'artifacts/npc-factory-object-flags.json').write_text(json.dumps(report,indent=2));print(report)
