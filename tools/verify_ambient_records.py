"""v180 ambient records: independent section walk versus owned C output."""
import json
import math
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]

def inspect(data):
    at = 0
    def take(n):
        nonlocal at
        if n < 0 or at + n > len(data):
            raise ValueError(('truncated', at, n))
        result = data[at:at+n]
        at += n
        return result
    count, = struct.unpack('<I', take(4))
    records, wire = [], bytearray(struct.pack('<I', count))
    for _ in range(count):
        start = at
        uid, x, y, z = struct.unpack('<I3f', take(16))
        header = take(1)[0]
        length, = struct.unpack('<H', take(2))
        name = take(length)
        if length >= 256 or b'\0' in name:
            raise ValueError('invalid name')
        near, volume, rolloff, flags = struct.unpack('<3fI', take(16))
        if not all(math.isfinite(v) for v in (x, y, z, near, volume, rolloff)):
            raise ValueError('nonfinite')
        size = at - start
        wire.extend(struct.pack('<5I3f256s3f', uid, header, flags, start, size,
                                x, y, z, name, near, volume, rolloff))
        records.append(dict(uid=uid, name=name.decode('cp1252'), position=[x, y, z],
                            header_byte=header, near_distance=near, volume=volume,
                            rolloff=rolloff, flags=flags, offset=start, bytes=size))
    if at != len(data):
        raise ValueError(('trailing', at, len(data)))
    return records, bytes(wire)

def main():
    levels = json.loads((root / 'artifacts/levels.json').read_text())
    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
    results = []
    for level in levels:
        section = next((s for s in level['sections'] if s['type'] == '0x500'), None)
        records, expected = [], struct.pack('<I', 0)
        if section:
            archive = next(a for a in inventory['files'] if a['path'] == level['archive'])
            entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
            with (root / 'Installed_Game' / level['archive']).open('rb') as stream:
                stream.seek(entry['offset'] + section['offset'] + 8)
                data = stream.read(section['size'])
            records, expected = inspect(data)
        run = subprocess.run([str(root / 'build/pc/Release/rf_level_entity_probe.exe'),
                              str(root / 'Installed_Game' / level['archive']),
                              level['file'], '--ambient'], capture_output=True, check=True)
        assert run.stdout == expected, level['file']
        owned_bytes = int(run.stderr) if run.stderr else 0
        assert owned_bytes == (12 + len(records) * 300 if section else 0)
        results.append(dict(file=level['file'], archive=level['archive'],
                            owner_bytes=owned_bytes, records=records))
    report = dict(result='PASS', levels=len(results),
                  records=sum(len(l['records']) for l in results),
                  peak_owner_bytes=max(l['owner_bytes'] for l in results), results=results,
                  scope='Original461ff0 read-order lead, independent Python section walk and '
                        'exact PC C records. Probe verifies budgets, truncation preservation, '
                        'archive-independent ownership and repeat close. Runtime sound '
                        'registration, playback and original loader execution excluded.')
    (root / 'artifacts/ambient-records.json').write_text(json.dumps(report, indent=2))
    print({k: v for k, v in report.items() if k != 'results'})

if __name__ == '__main__':
    main()
