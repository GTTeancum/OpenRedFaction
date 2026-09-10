"""Execute original 41e370 with real handle lookup, vector copy and wake flags.

Synthetic actor lists and registry records; no intercepted calls or game loop.
"""
import hashlib
import itertools
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

exe = root / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
mapped = pefile.PE(str(exe)).get_memory_mapped_image()
m = Uc(UC_ARCH_X86, UC_MODE_32)
m.mem_map(0x400000, (len(mapped) + 4095) // 4096 * 4096)
m.mem_write(0x400000, mapped)
base = 0x30000000
m.mem_map(base, 0x400000)
stack, stop = base + 0x3f0000, base + 0x3f1000
sentinel = 0x5cb060
word = lambda value: struct.pack('<I', value)
xp = pefile.PE(str(root / 'build/xbox/main.exe'))
xb = xp.get_memory_mapped_image()
xi = xp.OPTIONAL_HEADER.ImageBase
x = Uc(UC_ARCH_X86, UC_MODE_32)
x.mem_map(xi, (len(xb) + 4095) // 4096 * 4096)
x.mem_write(xi, xb)
x.mem_map(base, 0x400000)
entry = int(re.search(r'_rf_physics_support_refresh\s+([0-9a-fA-F]+)',
                     (root / 'build/xbox/main.map').read_text())[1], 16)
commands, outputs = bytearray(), bytearray()


def call():
    m.mem_write(stack, word(stop))
    m.reg_write(UC_X86_REG_ESP, stack)
    m.emu_start(0x41e370, stop, count=1000000)
    assert m.reg_read(UC_X86_REG_EIP) == stop


def compare_tick():
    before = [bytes(m.mem_read(actor, 0x1000)) for actor, _ in expected]
    call()
    for i, (actor, _) in enumerate(expected):
        mode, kind, _, _ = cases[i]
        source = bytes(m.mem_read(objects[i][0] + 0x144, 12))
        initial = before[i][0x8a0:0x8ac] + before[i][0x1a8:0x1ac] + before[i][0x7c:0x80]
        want = (bytes(m.mem_read(actor + 0x8a0, 12)) +
                bytes(m.mem_read(actor + 0x1a8, 4)) + bytes(m.mem_read(actor + 0x7c, 4)))
        commands.extend(word(mode) + word(kind == 'valid') + source + initial)
        outputs.extend(want)
        x.mem_write(base, source + initial)
        args = (stop, mode, base if kind == 'valid' else 0, base + 12, base + 24, base + 28)
        x.mem_write(stack, struct.pack('<6I', *args))
        x.reg_write(UC_X86_REG_ESP, stack)
        x.emu_start(entry, stop, count=10000)
        assert x.reg_read(UC_X86_REG_EIP) == stop
        assert bytes(x.mem_read(base + 12, 20)) == want, ('NXDK', i)


# Each valid object has a distinct slot and generation-checked runtime handle.
cases = list(itertools.product(
    (0, 1, 2, 3, 4, 8, 0xffffffff),
    ('valid', 'stale', 'empty', 'none'),
    ((0., 0., 0.), (2., -3., 4.), (-8., 16., -0.)),
    (0, 0xffffffff)))
m.mem_write(0x7394cc, bytes(4096))
expected, objects = [], []
updated = 0
for i, (mode, kind, velocity, flags) in enumerate(cases):
    actor = base + i * 0x1000
    obj = base + 0x200000 + i * 0x1000
    descriptor = actor + 0xf00
    handle = (7 << 16) | i
    record = bytearray((j * 13 + i) % 256 for j in range(0x1000))
    record[0x858:0x85c] = word(descriptor)
    record[0xf04:0xf08] = word(mode)
    record[0x28c:0x290] = word(actor + 0x1000 if i + 1 < len(cases) else sentinel)
    record[0x8ac:0x8b0] = word(handle if kind in ('valid', 'empty') else
                                    ((6 << 16) | i) if kind == 'stale' else 0xffffffff)
    record[0x8a0:0x8ac] = struct.pack('<3f', 11., 12., 13.)
    record[0x1a8:0x1ac] = word(flags)
    record[0x7c:0x80] = word(flags)
    object_record = bytearray(0x200)
    object_record[0x2c:0x30] = word(handle)
    object_record[0x144:0x150] = struct.pack('<3f', *velocity)
    m.mem_write(obj, bytes(object_record))
    if kind != 'empty':
        m.mem_write(0x7394cc + i * 4, word(obj))
    m.mem_write(actor, bytes(record))
    if mode in (1, 3) and kind == 'valid':
        record[0x8a0:0x8ac] = struct.pack('<3f', *velocity)
        record[0x1a8:0x1ac] = word(flags | 0x80000000)
        record[0x7c:0x80] = word(flags | 0x06000000)
        updated += 1
    expected.append((actor, bytes(record)))
    objects.append((obj, bytes(object_record)))

m.mem_write(0x5cb2ec, word(base))
registry = bytes(m.mem_read(0x7394cc, 4096))
compare_tick()
for i, (actor, record) in enumerate(expected):
    assert bytes(m.mem_read(actor, len(record))) == record, cases[i]
for obj, record in objects:
    assert bytes(m.mem_read(obj, len(record))) == record
assert bytes(m.mem_read(0x7394cc, 4096)) == registry
# A second tick must read changed object velocity, including a moving support
# becoming stationary, instead of retaining the first contact's cached value.
for i, (obj, record) in enumerate(objects):
    changed = bytearray(record)
    velocity = (9., -7., 5.) if cases[i][2] == (0., 0., 0.) else (0., 0., 0.)
    changed[0x144:0x150] = struct.pack('<3f', *velocity)
    m.mem_write(obj, bytes(changed))
    objects[i] = (obj, bytes(changed))
    if cases[i][0] in (1, 3) and cases[i][1] == 'valid':
        actor, record = expected[i]
        changed_actor = bytearray(record)
        changed_actor[0x8a0:0x8ac] = struct.pack('<3f', *velocity)
        expected[i] = (actor, bytes(changed_actor))
compare_tick()
for i, (actor, record) in enumerate(expected):
    assert bytes(m.mem_read(actor, len(record))) == record, ('second tick', cases[i])
for obj, record in objects:
    assert bytes(m.mem_read(obj, len(record))) == record
assert bytes(m.mem_read(0x7394cc, 4096)) == registry
# Empty-list sentinel must return without dereferencing actor fields.
m.mem_write(0x5cb2ec, word(sentinel))
call()
actual = subprocess.check_output([str(root / 'build/pc/Release/rf_physics_probe.exe'),
                                  '--support-refresh'], input=commands)
assert actual == outputs, 'PC support refresh differs from original'
report = dict(result='PASS', original_sha256=digest, actors=len(cases),
              refreshed_per_tick=updated, ticks=2, empty_list=True,
              scope='Unchanged original 41e370 with real 40a0e0 lookup, vector copy '
                    'and 40a420 wake flags. Full actor, support object and registry '
                    'bytes checked. Modes 1/3 only; stale/empty/absent handles '
                    'preserve cached velocity and flags; zero support velocity '
                    'still wakes eligible actors. Synthetic list/registry; no '
                    'frame scheduling or platform traversal claim. PC and compiled '
                    'NXDK helper match original velocity and both flag words for '
                    'all336 actor updates with caller-supplied lookup results.')
(root / 'artifacts/support-refresh-verification.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
