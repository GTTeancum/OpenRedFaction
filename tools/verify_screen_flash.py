"""Execute original full flash setter/damage wrapper against PC and NXDK code."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;stack=base+0xe000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
    pe=pefile.PE(str(path));data=pe.get_memory_mapped_image()
    m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096)
    m.mem_write(pe.OPTIONAL_HEADER.ImageBase,data);m.mem_map(base,65536)
    return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
original=machine(exe);xbox=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_screen_flash_set\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x416450);commands=bytearray();expected=bytearray()
for case in range(4096):
    damage=case%4==0
    values=(255,0,0,128) if damage else tuple(rng.getrandbits(32) for _ in range(4))
    owner=bytearray(rng.randbytes(0x1200));original.mem_write(base,bytes(owner))
    original.mem_write(0x7c75d4,pack(base))
    original.mem_write(stack,pack(stop,base,*values));original.reg_write(UC_X86_REG_ESP,stack)
    original.emu_start(0x4a7520 if damage else 0x416450,stop,count=1000)
    assert original.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(original.mem_read(base+0x10d0,8));owner[0x10d0:0x10d8]=result
    assert bytes(original.mem_read(base,len(owner)))==owner,'Unexpected owner mutation'
    # Guard bytes catch reconstruction writes outside its compact state.
    xbox.mem_write(base,bytes([0xa5])*32)
    xbox.mem_write(stack,pack(stop,base+8,*values));xbox.reg_write(UC_X86_REG_ESP,stack)
    xbox.emu_start(entry,stop,count=1000)
    assert xbox.reg_read(UC_X86_REG_EIP)==stop and xbox.reg_read(UC_X86_REG_EAX)==0
    assert bytes(xbox.mem_read(base,32))==bytes([0xa5])*8+result+bytes([0xa5])*16,case
    commands.extend(pack(*values));expected.extend(result)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--screen-flash'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',cases=4096,damage_wrapper_cases=1024,original_sha256=digest,
    scope='Full original416450 and50cc40;1024 cases enter4a7520 with prepared global player. Exact PC/NXDK RGBA byte truncation and separate full-width alpha, with owner/guard preservation. No decay, rendering or live player ownership.')
(root/'artifacts/screen-flash.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
