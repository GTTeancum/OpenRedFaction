"""Compare reconstructed non-linked eye updates to original x86 instructions."""
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path
import pefile
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW
binary = root/'Installed_Game/RF.exe'
assert hashlib.sha256(binary.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(binary)); image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096); u.mem_write(0x400000, image)
e, cls, info, stack, stop = 0x30000000, 0x30002000, 0x30003000, 0x31000000, 0x32000000
u.mem_map(e, 0x10000); u.mem_map(stack, 4096); u.mem_map(stop, 4096)
def word(address, value): u.mem_write(address, struct.pack('<I', value & 0xffffffff))
word(e+0x29c, cls); word(e+0x294, info)
rng = random.Random(194)
inputs, expected = [], []
for case in range(400):
    pos = [rng.uniform(-100, 100) for _ in range(3)]
    matrix = [rng.uniform(-1,1) for _ in range(9)]
    offsets = [rng.uniform(-2,2) for _ in range(6)]
    flags, tag = (0x20 if case % 17 == 0 else 0), (-1 if case % 19 == 0 else 3)
    current, previous = rng.randrange(12), rng.randrange(12)
    duration, elapsed = (0.0 if case % 7 == 0 else 0.5), rng.uniform(0,0.5)
    data = struct.pack('<18fIiii2f', *(pos+matrix+offsets), flags, tag, current, previous, duration, elapsed)
    inputs.append(data)
    u.mem_write(e+0x3c, data[:12]); u.mem_write(e+0x48, data[12:48])
    u.mem_write(cls+0x9c, data[48:72]); word(info+0x728, flags); word(cls+0x1ec, tag)
    word(e+0x138c, current); word(e+0x1390, previous); u.mem_write(e+0x1394, data[88:96])
    word(stack+4080, stop); word(stack+4084, e)
    u.reg_write(UC_X86_REG_ESP, stack+4080); u.reg_write(UC_X86_REG_FPCW, 0x37f)
    u.emu_start(0x4194e0, stop, count=3000)
    expected.append(struct.unpack('<3f', u.mem_read(e+0x7d4, 12)))
run = subprocess.run([str(root/'build/pc/Release/rf_eye_probe.exe')], input=b''.join(inputs), capture_output=True, check=True)
assert len(run.stdout) == len(inputs)*16
maximum = 0.0
for index, want in enumerate(expected):
    status, *actual = struct.unpack_from('<i3f', run.stdout, index*16)
    assert status == 0
    for a,b in zip(actual,want):
        maximum = max(maximum, abs(a-b))
        assert abs(a-b) <= 0.00002, (index, actual, want)
report = dict(cases=len(inputs), maximum_absolute_error=maximum, tolerance=0.00002,
              original='0x4194e0', scope='Non-linked branch, deterministic finite fixtures; no entity/model integration', result='PASS')
(root/'artifacts/eye-verification.json').write_text(json.dumps(report, indent=2))
print(report)
