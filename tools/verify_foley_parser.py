"""Execute original Foley parser primitives; no parser hooks or game input."""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP

exe = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe))
image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
base = 0x30000000
u.mem_map(base, 0x40000)
parser, text, needle, barrier, stack, stop = [base + x for x in (0, 0x1000, 0x20000, 0x21000, 0x30000, 0x3f000)]

def run(entry, data, token, boundary=None):
    assert len(data) < 0x1f000
    u.mem_write(parser, bytes(272))
    u.mem_write(parser, struct.pack('<II', text, text))
    u.mem_write(parser + 0x10c, struct.pack('<I', len(data)))
    u.mem_write(text, data + bytes(16))
    u.mem_write(needle, token + b'\0')
    u.mem_write(barrier, (boundary or b'') + b'\0')
    u.mem_write(stack, struct.pack('<III', stop, needle, barrier if boundary else 0))
    u.reg_write(UC_X86_REG_ECX, parser)
    u.reg_write(UC_X86_REG_ESP, stack)
    u.emu_start(entry, stop, count=1000000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    return (u.reg_read(UC_X86_REG_EAX) & 255,
            struct.unpack('<I', u.mem_read(parser + 4, 4))[0] - text,
            struct.unpack('<I', u.mem_read(parser + 0x108, 4))[0])

cases = [
    (0x511fc0, b'$Sound: surplus\r\n$Name: next', b'$Name:', None, (1, 17, 1)),
    (0x511fc0, b'$name: lower', b'$Name:', None, (0, 0, 0)),
    (0x511fc0, b'junk $Name: next', b'$Name:', None, (1, 5, 0)),
    (0x511fc0, b'#End\r\n$Name: next', b'$Name:', b'#End', (0, 0, 0)),
    (0x511fc0, b'  // comment\r\n$Name: next', b'$Name:', None, (1, 14, 1)),
    (0x5125c0, b' \r\n$sOuNdS: 4', b'$Sounds:', None, (1, 11, 1)),
    (0x5125c0, b' $Material: rock', b'$Sounds:', None, (0, 1, 0)),
    (0x5125c0, b'junk $Sounds: 4', b'$Sounds:', None, (0, 0, 0)),
]
for entry, data, token, boundary, expected in cases:
    actual = run(entry, data, token, boundary)
    assert actual == expected, (hex(entry), data, actual, expected)

inventory = json.loads((ROOT / 'artifacts/inventory.json').read_text())['files']
archive = next(a for a in inventory if a['path'] == 'tables.vpp')
record = next(e for e in archive['vpp']['entries'] if e['name'] == 'foley.tbl')
with (ROOT / 'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(record['offset'])
    foley = f.read(record['size'])
checks = []
for name in (b'Default Footstep', b'Solid Footstep'):
    start = re.search(rb'\$Name:\s*"' + re.escape(name) + rb'"', foley).start()
    end = foley.index(b'$Name:', start + 7)
    section = foley[start:end]
    rows = section.split(b'$Sound:')
    assert len(rows) == 7 and re.search(rb'\$Sounds:\s*4\b', rows[0])
    # Locate fifth sample: the loader has consumed exactly the first four.
    offset = start
    for _ in range(5):
        offset = foley.index(b'$Sound:', offset) + len(b'$Sound:')
    offset -= len(b'$Sound:')
    actual = run(0x511fc0, foley[offset:], b'$Name:')
    assert actual[0] == 1 and actual[1] == end - offset
    checks.append(dict(group=name.decode(), declared=4, listed=6,
                       skipped_samples=2, next_group_offset=end))
report = dict(result='PASS', original_sha256=digest, primitive_cases=len(cases),
              surplus_group_checks=checks,
              scope='Unhooked original 511fc0 search and 5125c0 optional-token consumption, including whitespace/comment and CRT callees. Actual Foley tails confirm surplus-row skipping. Does not execute full 434960 group loader or sample registration.')
(ROOT / 'artifacts/foley-parser.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
