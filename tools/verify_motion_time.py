"""Compare the original update prefix's elapsed-to-tick conversion, no hooks."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,descriptor,stack=0x30000000,0x30100000,0x31000000
for a in (obj,descriptor,stack): u.mem_map(a,65536)
u.mem_write(obj+0x12d0,struct.pack('<I',1)); u.mem_write(obj+0x1d50,struct.pack('<I',descriptor))
u.mem_write(descriptor+0xf58,struct.pack('<I',1))
rng=random.Random(0x51ba80)
values=[0,.2,-.2,1/30,1/60,1/120,1/4800,-1/4800]
values += [rng.uniform(-1000,1000) for _ in range(5000)]
# Nearest float and its neighbors around tick boundaries exercise truncation.
for n in range(1,1000):
    bits,=struct.unpack('<I',struct.pack('<f',n/4800))
    values.extend(struct.unpack('<f',struct.pack('<I',bits+d))[0] for d in (-1,0,1))
raw=b''.join(struct.pack('<f',v) for v in values)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--elapsed-ticks'],input=raw,capture_output=True,check=True)
assert len(run.stdout)==len(values)*8
for i in range(len(values)):
    u.mem_write(stack+64000,struct.pack('<I',0)+raw[i*4:i*4+4])
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_ECX,obj); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51ba80,0x51badc,count=1000)
    assert u.reg_read(UC_X86_REG_EIP)==0x51badc
    status,want=struct.unpack_from('<iI',run.stdout,i*8)
    assert status==0 and want==u.reg_read(UC_X86_REG_EAX),(i,status,want,u.reg_read(UC_X86_REG_EAX))
report=dict(result='PASS',samples=len(values),scope='Original update prefix through integer conversion, not full animation update')
(root/'artifacts/motion-time-verification.json').write_text(json.dumps(report,indent=2)); print(report)
