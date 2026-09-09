"""Execute original class constructor to separate assigned and static defaults."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image);u.mem_map(0,4096)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
fields={'use_kind':0x1b4,'primary_flags':0x724,'secondary_flags':0x728,'sphere_count':0xcec}
records=[]
for fill in (0,0xa5,0x5a):
 u.mem_write(base,bytes([fill])*0x1514);u.mem_write(stack,struct.pack('<I',stop))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x42d290,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 values={name:bytes(u.mem_read(base+offset,4)).hex() for name,offset in fields.items()}
 for name in ('use_kind','primary_flags','secondary_flags'):assert values[name]==(bytes([fill])*4).hex()
 assert values['sphere_count']=='00000000';records.append(dict(fill=fill,fields=values))
# Every static descriptor begins with zero bytes for the fields left untouched.
for index in range(75):
 for offset in (0x1b4,0x724,0x728):assert image[0x1cc500+index*0x1514+offset:0x1cc504+index*0x1514+offset]==bytes(4)
report=dict(result='PASS',constructor_cases=3,static_descriptors=75,records=records,scope='Original 42d290 with all callees unchanged. Use-kind and flag fields retain static zero storage until parser writes; sphere count is explicitly zeroed. Does not establish later loader or runtime mutations.')
(root/'artifacts/class-construction-inspection.json').write_text(json.dumps(report,indent=2));print(report)
