"""Original registration precedence; filesystem presence/metadata supplied."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;m.mem_map(base,65536)
u=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def hook(machine,address,size,user):
 sp=machine.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',machine.mem_read(sp,4))[0]
 machine.reg_write(UC_X86_REG_EAX,base+512 if address==0x56baa0 else 1)
 machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
for addr in (0x56baa0,0x544680):m.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
def call(addr,args):
 m.mem_write(stack,u(stop)+args);m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f)
 m.emu_start(addr,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
rows=[]
for first,second in [((10,.25,2),(5,1,1)),((5,1,1),(10,.25,2)),((0,.5,1),(20,.75,3))]:
 m.mem_write(0x1cfc5cc,u(0));m.mem_write(0x1cd3ba8,bytes(128));m.mem_write(base,b'DoorOpen_07.wav\0')
 assert call(0x5054b0,u(base)+f(*first))==0
 record=bytes(m.mem_read(0x1cd3ba8,64))
 assert struct.unpack('<f',record[32:36])[0]==first[1]
 assert struct.unpack('<f',record[36:40])[0]==(first[0] if first[0]>0 else 1)
 assert struct.unpack('<f',record[44:48])[0]==first[2]
 assert struct.unpack('<I',record[52:56])[0]==0
 m.mem_write(base,b'dooropen_07.WAV\0')
 assert call(0x543580,u(base)+f(*second)+u(3))==0
 assert bytes(m.mem_read(0x1cd3ba8,64))==record
 assert bytes(m.mem_read(0x1cfc5cc,4))==u(1)
 rows.append(dict(first=first,duplicate=second,record=record.hex()))
report=dict(result='PASS',original_sha256=digest,cases=len(rows),scope='Original5054b0 wrapper and543580 registration, comparison and far arithmetic unchanged. Filesystem lookup and presence supplied; case-insensitive duplicate preserves entire first record, including category. Does not establish global initialization order or table parser behavior.',rows=rows)
(root/'artifacts/audio-registration-verification.json').write_text(json.dumps(report,indent=2));print(report)
