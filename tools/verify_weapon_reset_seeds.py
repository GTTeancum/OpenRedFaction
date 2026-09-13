"""Execute original global weapon array construction and inspect reset flag seeds."""
import hashlib,json,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();reports=[]
for poison in (False,True):
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
 u.mem_map(0,0x1000);u.mem_write(0,struct.pack('<I',0xffffffff));u.mem_map(0x30000000,0x10000)
 if poison:
  for i in range(64):u.mem_write(0x85cd08+i*0x550+0x264,struct.pack('<II',0xa5000000+i,0x5a000000+i))
 before=[bytes(u.mem_read(0x85cd08+i*0x550+0x264,8)) for i in range(64)]
 u.mem_write(0x3000e000,struct.pack('<I',0x3000f000));u.reg_write(UC_X86_REG_ESP,0x3000e000)
 u.emu_start(0x4c2990,0x3000f000,count=2000000);assert u.reg_read(UC_X86_REG_EIP)==0x3000f000
 after=[bytes(u.mem_read(0x85cd08+i*0x550+0x264,8)) for i in range(64)]
 assert after==before
 if not poison:assert all(v==bytes(8) for v in after)
 assert bytes(u.mem_read(0,4))==struct.pack('<I',0xffffffff)
 reports.append(dict(poisoned=poison,descriptors=64,flags_unchanged=True))
report=dict(result='PASS',cases=reports,original_sha256=digest,scope='Actual4c2990 static array initializer,5736fb,64 full4c9f60 constructors and nested constructors execute unchanged. Loader-mapped zero data yields zero264/268 seeds; injected flags survive construction. No kernel/allocator hooks. Does not model reloading a previously populated weapon table.')
(root/'artifacts/weapon-reset-seeds.json').write_text(json.dumps(report,indent=2));print(report)
