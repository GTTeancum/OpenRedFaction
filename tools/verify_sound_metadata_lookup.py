"""Original Bluebeard comparator, full-table sort and metadata lookup.

Populate records from the independent inventory; execute sort/search without
hooks. This does not execute the original Bluebeard text parser.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

source = json.loads((root/'artifacts/sound-loop-inventory.json').read_text())
rows = source['rows']
path = root/'Installed_Game/RF.exe'
digest = hashlib.sha256(path.read_bytes()).hexdigest()
assert digest == source['original_sha256'] == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(path)); data = pe.get_memory_mapped_image()
m = Uc(UC_ARCH_X86, UC_MODE_32)
m.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(data)+4095)//4096*4096)
m.mem_write(pe.OPTIONAL_HEADER.ImageBase, data)
base = 0x30000000; stack = base+0xe000; stop = base+0xf000
m.mem_map(base, 65536)
table = 0x1fd0f28
word = lambda v: struct.pack('<I', v & 0xffffffff)
read = lambda addr: struct.unpack('<I', m.mem_read(addr, 4))[0]
def call(address, *args, limit=1000000):
    m.mem_write(stack, word(stop)+b''.join(word(a) for a in args))
    m.reg_write(UC_X86_REG_ESP, stack)
    m.emu_start(address, stop, count=limit)
    assert m.reg_read(UC_X86_REG_EIP) == stop, hex(m.reg_read(UC_X86_REG_EIP))
    assert m.reg_read(UC_X86_REG_ESP) == stack+4
    return m.reg_read(UC_X86_REG_EAX)

comparisons = 0
for left, right, expected in [
    ('a\\B.WAV', 'b.wav', 0), ('a/b.wav', 'b.wav', -1),
    ('b.wav', 'a\\b.wav', 0), ('x\\A.wav', 'y\\b.wav', -1),
    ('x\\b.wav', 'A.wav', 1), ('x\\', '', 0),
    ('a/b.wav', 'A/B.WAV', 0), ('a\\x/b.wav', 'x/b.wav', 0)]:
    m.mem_write(base, left.encode()+b'\0'); m.mem_write(base+256, right.encode()+b'\0')
    value = call(0x56bb80, base, base+256)
    signed = value if value < 0x80000000 else value-0x100000000
    assert (signed > 0)-(signed < 0) == expected, (left, right, signed)
    comparisons += 1

records = bytearray(4096*180)
for i, row in enumerate(rows):
    name = row['path'].encode('cp1252'); assert len(name) < 120
    records[i*180:i*180+len(name)] = name
    # Tag an otherwise unused word to track sort permutation, not runtime data.
    records[i*180+164:i*180+168] = word(i+1)
    records[i*180+168:i*180+172] = word((int(row['looping'])<<30) | (row['loop_start'] & 0x7ffffff))
    records[i*180+172:i*180+176] = word(0x10000000)
m.mem_write(table, bytes(records))
call(0x5749fa, table, 4096, 180, 0x56bb80, limit=100000000)
sorted_records = bytes(m.mem_read(table, len(records)))
tags = [struct.unpack_from('<I', sorted_records, i*180+164)[0] for i in range(4096)]
assert sorted(tags) == [0]*(4096-len(rows))+list(range(1, len(rows)+1))
keys = [rows[tag-1]['name'].lower() if tag else '' for tag in tags]
assert keys == sorted(keys)
m.mem_write(0x1fce720, b'\x01')
results = {}
for row in rows:
    name = row['name']
    m.mem_write(base, name.encode('cp1252')+b'\0')
    found = call(0x56baa0, base)
    assert table <= found < table+len(records) and (found-table)%180 == 0
    tag = read(found+164); selected = rows[tag-1]
    assert selected['name'].lower() == name.lower()
    results[name.lower()] = dict(path=selected['path'], source_index=tag-1,
                                sorted_index=(found-table)//180, looping=selected['looping'])
    # Sorted comparator strips backslash prefixes on both search and row.
    m.mem_write(base, ('arbitrary\\'+name.swapcase()).encode('cp1252')+b'\0')
    assert call(0x56baa0, base) == found
for missing in ('', 'missing-metadata.wav', 'arbitrary/'+rows[0]['name']):
    m.mem_write(base, missing.encode()+b'\0'); assert call(0x56baa0, base) == 0
# The unsorted branch intentionally uses full-string comparison instead.
m.mem_write(table, bytes(records)); m.mem_write(0x1fce720, b'\0')
m.mem_write(base, rows[0]['name'].encode()+b'\0'); assert call(0x56baa0, base) == 0
m.mem_write(base, rows[0]['path'].swapcase().encode()+b'\0'); assert call(0x56baa0, base) == table
duplicates = {name: results[name] for name in source['ambiguous_basenames']}
report = dict(result='PASS', records=len(rows), comparator_cases=comparisons,
              lookups=len(rows)*2+5, duplicate_selections=duplicates,
              original_sha256=digest, sorted_source_tags=tags, selected=results,
              scope='Unhooked original56bb80 comparator,5749fa sort of4096 records, '
                    '56baa0 sorted and unsorted search including original CRT callees. '
                    'Record names/loop fields supplied from inventory; parser and runtime loading not executed.')
(root/'artifacts/sound-metadata-lookup.json').write_text(json.dumps(report, indent=2))
print({key: value for key, value in report.items() if key not in ('sorted_source_tags', 'selected')})

