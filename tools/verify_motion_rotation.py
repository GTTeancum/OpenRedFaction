"""Exhaustively compare signed rotation component decoding to RF.exe 0x417e90."""
import hashlib, json, struct, subprocess, sys
from pathlib import Path
import pefile
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_FPCW
exe = root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
data, stack, stop = 0x30000000, 0x31000000, 0x32000000
for a in (data,stack,stop): u.mem_map(a,4096)
# Each component independently covers every signed 16-bit value, with different
# permutations to catch offsets and component-order errors.
packed = [struct.pack('<4H', n, n ^ 0x8000, 65535-n, (n*17)&65535) for n in range(65536)]
run = subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--decode-rotation'],
                     input=b''.join(packed),capture_output=True,check=True)
assert len(run.stdout)==len(packed)*20
for i, raw in enumerate(packed):
    u.mem_write(data,raw)
    u.mem_write(stack+4000,struct.pack('<2I',stop,data))
    u.reg_write(UC_X86_REG_ESP,stack+4000); u.reg_write(UC_X86_REG_ECX,data+32)
    u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x417e90,stop,count=100)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    want=bytes(u.mem_read(data+32,16))
    status,=struct.unpack_from('<i',run.stdout,i*20)
    assert status==0 and run.stdout[i*20+4:(i+1)*20]==want,(i,status)
report=dict(result='PASS',vectors=len(packed),component_comparisons=len(packed)*4,
            scope='All signed 16-bit component values; unhooked original 0x417e90, no interpolation')
(root/'artifacts/motion-rotation-decode.json').write_text(json.dumps(report,indent=2))
print(report)
