"""Inventory Bluebeard loop declarations and execute original metadata writes.

Token recognition/integer reads are supplied; this is not a full parser proof.
Basename joins below are inventory joins, not reconstructed filesystem lookup.
"""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP

data = (root/'Installed_Game/bluebeard.bty').read_bytes()
text = '\n'.join(line.split('//', 1)[0].strip() for line in data.decode('cp1252').splitlines())
rows = []
for block in re.split(r'(?m)^\$Sound:\s*', text)[1:]:
    lines = [line for line in block.splitlines() if line]
    name = re.fullmatch(r'"([^"\r\n]+)"', lines[0])
    assert name, lines[0]
    assert lines.count('+Looping Sound') <= 1
    looping = '+Looping Sound' in lines
    offsets = [re.fullmatch(r'\+Loop Start:\s*(-?\d+)', line) for line in lines if line.startswith('+Loop Start')]
    assert len(offsets) == int(looping) and all(offsets)
    offset = int(offsets[0][1]) if offsets else 0
    rows.append(dict(path=name[1], name=name[1].replace('\\', '/').split('/')[-1],
                     looping=looping, loop_start=offset))
by_name = {}
for row in rows:
    by_name.setdefault(row['name'].lower(), []).append(row)
ambiguous = {key: values for key, values in by_name.items() if len(values) > 1}

exe = root/'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe)); image = pe.get_memory_mapped_image()
m = Uc(UC_ARCH_X86, UC_MODE_32)
m.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(image)+4095)//4096*4096)
m.mem_write(pe.OPTIONAL_HEADER.ImageBase, image)
base = 0x30000000; stack = base+0xe000
m.mem_map(base, 65536)
word = lambda v: struct.pack('<I', v & 0xffffffff)
read = lambda address: struct.unpack('<I', m.mem_read(address, 4))[0]
calls = []
def hook(machine, address, size, context):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if address in (0x5125c0, 0x5126a0):
        token = read(sp+4)
        assert token == (0x5aad80 if address == 0x5125c0 else 0x5aad90)
        result, cleanup = int(looping), 4  # thiscall parser consumes token.
    else:
        result, cleanup = offset, 0
    calls.append(address)
    machine.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
    machine.reg_write(UC_X86_REG_ESP, sp+4+cleanup)
    machine.reg_write(UC_X86_REG_EIP, read(sp))
for address in (0x5125c0, 0x5126a0, 0x512750):
    m.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

cases = [(r['looping'], r['loop_start'], 0) for r in rows]
cases += [(enabled, value, flags) for enabled in (False, True)
          for value in (-1, 0, 1, 0x7ffffff, 0x8000000, 0x7fffffff)
          for flags in (0, 0x80000000, 0xffffffff)]
for looping, offset, initial in cases:
    before = bytearray([0xa5]*180); before[168:172] = word(initial)
    m.mem_write(base, bytes(before)); calls.clear()
    m.reg_write(UC_X86_REG_EBP, base); m.reg_write(UC_X86_REG_ESP, stack)
    m.emu_start(0x56bed8, 0x56bf2b, count=100)
    assert m.reg_read(UC_X86_REG_EIP) == 0x56bf2b
    assert m.reg_read(UC_X86_REG_ESP) == stack
    expected = ((initial | 0x40000000) & ~0x7ffffff) | (offset & 0x7ffffff) if looping else initial
    before[168:172] = word(expected)
    assert bytes(m.mem_read(base, 180)) == before
    assert calls == ([0x5125c0, 0x5126a0, 0x512750] if looping else [0x5125c0])

levels = json.loads((root/'artifacts/ambient-records.json').read_text())['results']
joins = []
for level in levels:
    for ambient in level['records']:
        matches = by_name.get(ambient['name'].lower(), [])
        metadata = matches[0] if len(matches) == 1 else None
        joins.append(dict(level=level['file'], uid=ambient['uid'], name=ambient['name'],
                          looping=metadata['looping'] if metadata else None, matches=len(matches)))
summary = dict(result='PASS', records=len(rows), looping=sum(r['looping'] for r in rows),
               nonzero_loop_starts=sum(r['loop_start'] != 0 for r in rows),
               original_write_cases=len(cases), ambient_records=len(joins),
               ambient_looping=sum(j['looping'] is True for j in joins),
               ambient_nonlooping=sum(j['looping'] is False for j in joins),
               ambient_missing_metadata=sum(j['matches'] == 0 for j in joins),
               ambient_ambiguous_metadata=sum(j['matches'] > 1 for j in joins),
               duplicate_basenames=len(ambiguous))
report = dict(summary, original_sha256=digest, bluebeard_sha256=hashlib.sha256(data).hexdigest(),
              rows=rows, ambiguous_basenames=ambiguous, ambient_joins=joins,
              scope='Installed Bluebeard inventory; original56bed8..56bf2b metadata writes '
                    'with token/integer boundaries supplied. Whole180-byte record preservation. '
                    'Unique basename inventory join only; full parser, name normalization, '
                    'registration and audible integration are not proved.')
(root/'artifacts/sound-loop-inventory.json').write_text(json.dumps(report, indent=2))
print(summary)
