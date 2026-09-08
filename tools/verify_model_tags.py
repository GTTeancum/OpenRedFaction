"""Compare shared C tag lookup against unhooked original character lookup."""
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_EIP

binary = root / 'Installed_Game/RF.exe'
assert hashlib.sha256(binary.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(binary)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
obj, stack, stop = 0x30000000, 0x31000000, 0x32000000
u.mem_map(obj, 0x10000); u.mem_map(stack, 0x1000); u.mem_map(stop, 0x1000)
def word(a, v): u.mem_write(a, struct.pack('<I', v & 0xffffffff))
def string(a, v): u.mem_write(a, v + b'\0')

# Explicit empty, duplicate, each-group, ASCII-folding and non-ASCII cases.
fixtures = [([[], [], []], b'eye'), ([[b'eye'], [b'EYE'], [b'eye']], b'eYe'),
            ([[], [b'eye'], []], b'EYE'), ([[], [], [b'eye']], b'EYE'),
            ([[b''], [], []], b''), ([[b'\xc0'], [], [b'\xe0']], b'\xe0')]
rng = random.Random(0x51d5b0)
pool = [b'eye', b'EYE', b'root', b'hand', b'head', b'tag_1', b'eye_extra', b'\xc0', b'\xe0', b'']
for _ in range(300):
    groups = [[rng.choice(pool) for _ in range(rng.randrange(17))] for _ in range(3)]
    fixtures.append((groups, rng.choice(pool + [b'missing', b'eYe', b'ROOT'])))
inputs, expected = [], []
for groups, query in fixtures:
    # Rebuild the three original groups and pointer chain for each fixture.
    word(obj + 0x48, len(groups[0])); word(obj + 0x12b8, len(groups[1]))
    word(obj + 0x1a50, obj + 0x2000)
    word(obj + 0x208c, obj + 0x2200); word(obj + 0x2204, obj + 0x2400)
    word(obj + 0x2410, obj + 0x3000); word(obj + 0x2414, len(groups[2]))
    for i, name in enumerate(groups[0]): string(obj + 0x4c + i * 0x4c, name)
    for i, name in enumerate(groups[1]):
        word(obj + 0x12bc + i * 0x38, obj + 0x4000 + i * 32)
        string(obj + 0x4000 + i * 32, name)
    for i, name in enumerate(groups[2]): string(obj + 0x3000 + i * 100, name)
    string(obj + 0x5000, query)
    word(stack + 0xff0, stop); word(stack + 0xff4, obj + 0x5000)
    word(0x20852f4, 0)  # Explicitly test the CRT default-locale branch.
    u.reg_write(UC_X86_REG_ESP, stack + 0xff0); u.reg_write(UC_X86_REG_ECX, obj)
    u.emu_start(0x51d5b0, stop, count=100000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    result = u.reg_read(UC_X86_REG_EAX)
    expected.append(-1 if result == 0xffffffff else result)
    data = struct.pack('<3I', *(len(g) for g in groups))
    for group in groups:
        data += b''.join(name.ljust(32, b'\0') for name in group)
        data += bytes((16 - len(group)) * 32)
    inputs.append(data + query.ljust(32, b'\0'))
run = subprocess.run([str(root / 'build/pc/Release/rf_model_probe.exe')],
                     input=b''.join(inputs), capture_output=True, check=True)
assert len(run.stdout) == len(fixtures) * 8
for i, want in enumerate(expected):
    status, actual = struct.unpack_from('<ii', run.stdout, i * 8)
    assert actual == want and status == (-3 if want == -1 else 0), (i, want, actual, status)
report = dict(result='PASS', cases=len(fixtures), original='0x51d5b0',
              original_comparator='0x57c130, unhooked default-locale branch',
              scope='Synthetic valid name groups; model asset parsing and pose evaluation not covered')
(root / 'artifacts/model-tag-verification.json').write_text(json.dumps(report, indent=2))
print(report)
