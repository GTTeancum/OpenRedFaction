"""Prepared original airborne steering vs PC and compiled NXDK, no hooks."""
import hashlib
import json
import random
import re
import struct
import subprocess
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW

base, stack, stop = 0x30000000, 0x3000e000, 0x3000f000
words = lambda *v: struct.pack('<' + 'I' * len(v), *v)
floats = lambda *v: struct.pack('<' + 'f' * len(v), *v)


def machine(path):
    p = pefile.PE(str(path)); blob = p.get_memory_mapped_image(); origin = p.OPTIONAL_HEADER.ImageBase
    m = Uc(UC_ARCH_X86, UC_MODE_32)
    m.mem_map(origin, (len(blob) + 4095) // 4096 * 4096); m.mem_write(origin, blob)
    m.mem_map(base, 65536)
    return m


exe = root / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u, x = machine(exe), machine(root / 'build/xbox/main.exe')
entry = int(re.search(r'_rf_physics_air_steer\s+([0-9a-fA-F]+)',
                     (root / 'build/xbox/main.map').read_text())[1], 16)
rng = random.Random(0x49e7ca)
commands, expected = bytearray(), bytearray()
for case in range(4096):
    values = [rng.choice((0., 1/60, 1/30, .1)), rng.choice((0., .5, 1.)),
              rng.choice((0., 1., 10., 100.)), rng.choice((0., 1., 5., 20.))]
    values += [rng.uniform(-150, 150) for _ in range(3)]
    values += [rng.uniform(-30, 30) for _ in range(3)]
    packed = floats(*values); values = struct.unpack('<10f', packed)
    flags = 0x200000 if case % 2 else 0
    u.mem_write(base, bytes(0x3000)); u.mem_write(stack, bytes(256))
    u.mem_write(base + 0x294, words(base + 0x2000))
    u.mem_write(base + 0x205c, packed[8:12]); u.mem_write(base + 0x2050, packed[12:16])
    u.mem_write(base + 0x1488, packed[12:16])
    u.mem_write(base + (0x2050 if flags else 0x1488), floats(999.))
    u.mem_write(base + 0x1b0, packed[:4]); u.mem_write(0x5a00e0, packed[4:8])
    u.mem_write(base + 0x144, packed[28:40]); u.mem_write(base + 0x1a8, words(flags))
    u.mem_write(stack + 12, packed[16:28])
    original = bytes(u.mem_read(base, 0x3000))
    u.reg_write(UC_X86_REG_ESP, stack); u.reg_write(UC_X86_REG_ESI, base)
    u.reg_write(UC_X86_REG_FPCW, 0x37f)
    u.emu_start(0x49e7ca, 0x49e8b7, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x49e8b7
    want = bytes(u.mem_read(base + 0x144, 12))
    preserved = bytearray(original); preserved[0x144:0x150] = want
    assert bytes(u.mem_read(base, 0x3000)) == preserved
    assert want[4:8] == packed[32:36], 'Vertical velocity changed'
    for repeat in (False, True):
        shared_flags = flags | (0x1000000 if repeat else 0)
        result = packed[28:40] if repeat else want
        commands.extend(packed + words(shared_flags)); expected.extend(result)
        state = bytearray(308); state[184:196] = packed[28:40]; state[272:276] = words(shared_flags)
        x.mem_write(base, bytes(state)); x.mem_write(base + 0x1000, packed[16:28])
        x.mem_write(stack, words(stop, base) + packed[:16] + words(base + 0x1000))
        x.reg_write(UC_X86_REG_ESP, stack); x.reg_write(UC_X86_REG_FPCW, 0x27f)
        x.emu_start(entry, stop, count=100000)
        assert x.reg_read(UC_X86_REG_EIP) == stop and x.reg_read(UC_X86_REG_EAX) == 0
        state[184:196] = result
        assert bytes(x.mem_read(base, 308)) == state, ('NXDK', case, repeat, want.hex(), bytes(x.mem_read(base+184,12)).hex())
actual = subprocess.check_output([str(root / 'build/pc/Release/rf_physics_probe.exe'), '--air-steer'], input=commands)
assert actual == expected, 'PC airborne steering mismatch'
report = dict(result='PASS', original_cases=4096, shared_cases=8192, original_sha256=digest,
              scope='Prepared 49e7ca..49e8b7 with unchanged norm/scaling callees. '
                    'PC/NXDK exact velocity and preserved body for both cap branches, '
                    'zero/nonzero limits and repeated-pass no-op. Transform, cap selection, '
                    'air-control configuration, scene integration and native XEMU excluded.')
(root / 'artifacts/air-steer-verification.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
