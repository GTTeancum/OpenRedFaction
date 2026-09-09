"""Check original support-query decisions; no world-query or gameplay claim."""
import hashlib
import itertools
import json
import struct
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EBX, UC_X86_REG_EIP

exe = ROOT / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
base, stack, descriptor, cls = 0x30000000, 0x30008000, 0x30006000, 0x30004000
u.mem_map(base, 0x10000)
word = lambda n: struct.pack('<I', n & 0xffffffff)

def stop_at_decision(uc, address, size, context):
    if address in (0x487fc0, 0x487fc9):
        uc.emu_stop()

u.hook_add(UC_HOOK_CODE, stop_at_decision)
cases = 0
for mode, attached, moved, moving_support, actor_bit8, category, material in itertools.product(
        (1, 2, 3, 8), (False, True), (False, True), (False, True), (False, True), (0, 1), (-1, 1)):
    u.mem_write(base, bytes(0x7000))
    # Type zero resolves its category through class +1b4 in original 486c90.
    for offset, value in ((0x24, 0), (0x294, cls), (0x858, descriptor),
                          (0x200, 7 if attached else -1), (0x7c, 8 if actor_bit8 else 0),
                          (0x1a8, 0x400000 if moving_support else 0), (0x1380, material)):
        u.mem_write(base + offset, word(value))
    u.mem_write(cls + 0x1b4, word(category))
    u.mem_write(descriptor + 4, word(mode))
    u.mem_write(stack, word(base))  # Argument already pushed by 487f78.
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_ESI, base)
    u.reg_write(UC_X86_REG_EBX, int(moved))
    u.emu_start(0x487f82, 0x487fd0, count=10000)
    end = u.reg_read(UC_X86_REG_EIP)
    assert end in (0x487fc0, 0x487fc9), hex(end)
    expected = mode in (3, 8) or (category == 1 and material == -1) or (
        mode == 1 and not attached and (moved or moving_support or actor_bit8))
    assert (end == 0x487fc0) == expected, (mode, attached, moved, moving_support, actor_bit8, category, material)
    cases += 1

report = dict(status='PASS', original_cases=cases,
    scope='Original 487f82 decision with unchanged 42a020, 429990, 486c90 and 4895d0 callees; stops before world query. Prepared category, descriptor and movement inputs; no live ledge traversal.',
    findings=['4895d0 tests actor flag 8, not moving-support identity.',
              'Modes 3/8 and category 1 with absent material bypass the ordinary grounded movement gate.',
              'Ordinary mode 1 requires no attachment and movement, physics moving-support flag, or actor flag 8.'])
(ROOT / 'artifacts/actor-support-gate-verification.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
