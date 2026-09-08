"""Compare full original material constructor, including unchanged runtime helpers."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
obj,stack=0x30000000,0x30100000
u.mem_map(0,4096);u.mem_map(obj,65536);u.mem_map(stack,65536)
rng=random.Random(0x54a7c0);cases=[bytes([v])*200 for v in (0,255,165)]+[rng.randbytes(200) for _ in range(997)]
out=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--material-init'],input=b''.join(cases));assert len(out)==len(cases)*200
for k,raw in enumerate(cases):
    u.mem_write(obj,bytes([0xcc])*256);u.mem_write(obj+16,raw)
    stop=stack+65000;u.mem_write(stack+64000,struct.pack('<I',stop));u.mem_write(0,struct.pack('<I',0x12345678))
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj+16)
    u.emu_start(0x54a7c0,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==obj+16
    assert bytes(u.mem_read(obj+16,200))==out[k*200:k*200+200],k
    assert bytes(u.mem_read(obj,16))==bytes([0xcc])*16 and bytes(u.mem_read(obj+216,40))==bytes([0xcc])*40
    assert bytes(u.mem_read(0,4))==struct.pack('<I',0x12345678)
report=dict(result='PASS',cases=len(cases),scope='Complete 54a7c0 including original SEH, vector constructor and color helpers; all 200 bytes, canaries and SEH restoration checked; material initialization only')
(root/'artifacts/model-material-init-verification.json').write_text(json.dumps(report,indent=2));print(report)
