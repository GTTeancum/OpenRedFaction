"""Resolve entity character names with original 0x5142d0 and audit VPP assets.

The original game inputs are read-only. Filename resolution executes original
x86 code in Unicorn without hooks; this does not load or evaluate the models.
"""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

exe = root / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
data, stack, stop = 0x30000000, 0x31000000, 0x32000000
for address in (data, stack, stop): u.mem_map(address, 4096)
def resolve(name):
    raw = name.encode('cp1252')
    assert len(raw) < 250 and b'\0' not in raw
    u.mem_write(data, bytes(4096)); u.mem_write(data, raw + b'\0')
    u.mem_write(data + 512, b'.v3c\0')
    u.mem_write(stack + 4000, struct.pack('<4I', stop, data + 1024, data, data + 512))
    u.reg_write(UC_X86_REG_ESP, stack + 4000)
    u.emu_start(0x5142d0, stop, count=100000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    return bytes(u.mem_read(data + 1024, 512)).split(b'\0', 1)[0].decode('cp1252')

inventory = json.loads((root / 'artifacts/inventory.json').read_text())
entries = {}
for archive in inventory['files']:
    for entry in archive.get('vpp', {}).get('entries', []):
        entries.setdefault(entry['name'].lower(), []).append((archive['path'], entry))
def read(archive, entry):
    with (root / 'Installed_Game' / archive).open('rb') as stream:
        stream.seek(entry['offset']); contents = stream.read(entry['size'])
    assert len(contents) == entry['size']
    return contents
table_matches = entries['entity.tbl']
assert len(table_matches) == 1, 'Resolve archive precedence before choosing a table'
table = read(*table_matches[0]).decode('cp1252')
table = '\n'.join(line.split('//', 1)[0] for line in table.splitlines())
names = sorted(set(re.findall(r'\$V3D Filename:\s*"([^"]+\.vcm)"', table, re.I)))
records = []
for name in names:
    resolved = resolve(name)
    matches = []
    for archive, entry in entries.get(resolved.lower(), []):
        contents = read(archive, entry)
        matches.append(dict(archive=archive, name=entry['name'], bytes=len(contents),
                            magic=contents[:4].decode('ascii', errors='replace'),
                            sha256=hashlib.sha256(contents).hexdigest()))
    records.append(dict(table_name=name, resolved=resolved, matches=matches))
assert resolve('miner.vcm') == 'miner.v3c'
report = dict(original_resolver='0x5142d0, called with .v3c by 0x51ce60',
              scope='Entity table character asset existence and headers; no mesh/pose loading',
              cases=[dict(input=n, output=resolve(n)) for n in
                     ['miner.vcm', 'miner', 'a.b.vcm', 'dir.with.dot/miner.vcm', 'dir.with.dot/miner', '.vcm']],
              models=records)
(root / 'artifacts/model-assets.json').write_text(json.dumps(report, indent=2))
print(f'{len(records)} character references; {sum(bool(r["matches"]) for r in records)} resolved to installed assets')
print('Missing:', [r['table_name'] for r in records if not r['matches']])
print('Resolver cases:', report['cases'])
