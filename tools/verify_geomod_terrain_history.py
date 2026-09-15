"""Original crater-history initialization and save-state record restoration snippets."""
import hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
exe=R/'Installed_Game/RF.exe';im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,0x40000);S=B+0x3e000;stop=B+0x3f000;w=lambda *v:struct.pack('<'+'I'*len(v),*v);rows=[]
for old in [1,8,128]:
 u.mem_write(0x647c9c,w(old));u.mem_write(S,w(0,0,0,stop));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x466aa3,stop,count=10000);assert struct.unpack('<I',u.mem_read(0x647c9c,4))[0]==0;rows.append(dict(old_count=old,reset_count=0))
restore=[]
for count in [0,1,8,128]:
 data=bytes((i*31+count)%256 for i in range(count*32));u.mem_write(B+0x17388,data);u.mem_write(B+0x1871b,bytes([count]));u.reg_write(UC_X86_REG_EBP,B);u.emu_start(0x4b4aa5,0x4b4ad0,count=10000);assert struct.unpack('<I',u.mem_read(0x647c9c,4))[0]==count;assert bytes(u.mem_read(0x648600,count*32))==data;restore.append(dict(count=count,record_bytes=count*32))
(R/'artifacts/crater-shading-re/terrain-history.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope=__doc__,reset_cases=rows,restore_cases=restore),indent=2));print('PASS:3 original history resets +4 count/record restoration cases')

