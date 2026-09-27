"""List authored L1S1 items around the disconnected stair navigation gap."""

import json
from pathlib import Path
import struct


root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
level = next(value for value in json.loads((root / 'artifacts/levels.json').read_text())
             if value['file'].lower() == 'l1s1.rfl')
archive = next(value for value in inventory['files']
               if value['path'] == level['archive'])
entry = next(value for value in archive['vpp']['entries']
             if value['name'] == level['file'])
section = next(value for value in level['sections']
               if value['type'] == '0x40000')
with (root / 'Installed_Game' / level['archive']).open('rb') as stream:
    stream.seek(entry['offset'] + section['offset'] + 8)
    data = stream.read(section['size'])
count, = struct.unpack_from('<I', data)
cursor = 4


def string():
    global cursor
    size, = struct.unpack_from('<H', data, cursor)
    cursor += 2
    value = data[cursor:cursor + size].decode('cp1252')
    cursor += size
    return value


rows = []
for _ in range(count):
    uid, = struct.unpack_from('<I', data, cursor)
    cursor += 4
    class_name = string()
    position = struct.unpack_from('<3f', data, cursor)
    cursor += 48
    script = string()
    cursor += 1
    quantity_bits, respawn, team = struct.unpack_from('<I2f', data, cursor)
    cursor += 12
    rows.append(dict(uid=uid, name=class_name, position=position,
                     script=script, quantity_bits=quantity_bits))
assert cursor == len(data)
near = [row for row in rows if -75 < row['position'][0] < 10
        and 10 < row['position'][2] < 55]
print(json.dumps(dict(total=len(rows), nearby=near), indent=2))
