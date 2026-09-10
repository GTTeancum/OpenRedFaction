"""Execute real Set_Gravity virtual on/off handlers against shared PC/NXDK.

The original link propagation is deliberately outside this action fixture.
"""
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW


def words(*values):
    return struct.pack('<' + 'I' * len(values), *(v & 0xffffffff for v in values))


def machine(path):
    pe = pefile.PE(str(path))
    image = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    pe.close()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(origin, (len(image) + 4095) // 4096 * 4096)
    cpu.mem_write(origin, image)
    cpu.mem_map(0x30000000, 65536)
    return cpu


original = root / 'Installed_Game/RF.exe'
sha = hashlib.sha256(original.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u = machine(original)
x = machine(root / 'build/xbox/main.exe')
base, stack, stop = 0x30000000, 0x3000e000, 0x3000f000
initial = words(0x411ccccd, 0, 0xc11ccccd, 0)
entries = struct.unpack('<2I', u.mem_read(0x589b40, 8))
assert entries == (0x4bcc00, 0x4b9f80)
rng = random.Random(0x4bcc00)
values = [0, 0x80000000, 0x40800000, 0x40400000, 0x411ccccd,
          1, 0x80000001, 0x7f7fffff, 0xff7fffff]
while len(values) < 1024:
    value = rng.getrandbits(32)
    if value & 0x7f800000 != 0x7f800000:
        values.append(value)
commands, expected = bytearray(), bytearray()
for value in values:
    for action in (0, 1):
        state = bytearray(0x2bc)
        struct.pack_into('<I', state, 0, 0x589b3c)
        struct.pack_into('<I', state, 0x290, 44)
        struct.pack_into('<I', state, 0x2b8, value)
        u.mem_write(base, bytes(state))
        u.mem_write(0x5a00dc, initial[:4])
        u.mem_write(0x7c7058, initial[4:])
        u.mem_write(0x62f2c8, words(0x40a362be))
        u.mem_write(stack, words(stop))
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_ECX, base)
        u.reg_write(UC_X86_REG_FPCW, 0x37f)
        u.emu_start(entries[0 if action else 1], stop, count=10000)
        assert u.reg_read(UC_X86_REG_EIP) == stop
        assert u.reg_read(UC_X86_REG_ESP) == stack + 4
        assert bytes(u.mem_read(base, len(state))) == bytes(state)
        assert bytes(u.mem_read(0x62f2c8, 4)) == words(0x40a362be)
        gravity = bytes(u.mem_read(0x5a00dc, 4)) + bytes(u.mem_read(0x7c7058, 12))
        assert gravity == (words(value, 0, value ^ 0x80000000, 0) if action else initial)
        commands.extend(words(value, action))
        expected.extend(words(0) + gravity)
# Shared propagation action is a no-op; invalid actions and nonfinite on
# payloads are defensive API behavior, not additional original parity claims.
for value, action, status in [(0x40400000, 2, 0), (0x40400000, 3, -4),
                              (0x7f800000, 1, -4), (0xff800000, 1, -4),
                              (0x7fc00000, 1, -4), (0x7fc00000, 0, 0)]:
    commands.extend(words(value, action))
    expected.extend(words(status) + initial)
probe = root / 'build/pc/Release/rf_event_probe.exe'
assert subprocess.check_output([str(probe), '--gravity-action'], input=commands) == expected
mapping = (root / 'build/xbox/main.map').read_text()
entry = int(re.search(r'_rf_event_gravity_action\s+([0-9a-fA-F]+)', mapping)[1], 16)
for i in range(len(commands) // 8):
    value, action = struct.unpack_from('<2I', commands, i * 8)
    x.mem_write(base, initial)
    x.mem_write(stack, words(stop, base, value, action))
    x.reg_write(UC_X86_REG_ESP, stack)
    x.emu_start(entry, stop, count=10000)
    assert x.reg_read(UC_X86_REG_EIP) == stop
    assert x.reg_read(UC_X86_REG_ESP) == stack + 4
    got = words(x.reg_read(UC_X86_REG_EAX)) + bytes(x.mem_read(base, 16))
    assert got == expected[i * 20:(i + 1) * 20], i
report = dict(result='PASS', original_action_cases=2048, shared_cases=2054,
              original_sha256=sha,
              pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
              nxdk_sha256=hashlib.sha256((root / 'build/xbox/main.exe').read_bytes()).hexdigest(),
              scope='Actual type-44 virtual on/off handlers; gravity state, unchanged event storage and jump impulse. Registry, scheduling, link propagation and campaign trigger wiring excluded.')
(root / 'artifacts/gravity-event-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(report)
