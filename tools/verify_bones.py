"""Check raw BONE field decoding against installed character payloads.

Section discovery is diagnostic signature scanning, not a full V3C directory
reader. The runtime decoder consumes only a caller-identified payload.
"""
import json
import struct
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
folder = root / 'artifacts/bone-tests'; folder.mkdir(parents=True, exist_ok=True)
probe = root / 'build/pc/Release/rf_bone_probe.exe'
records = []
for archive in inventory['files']:
    for entry in archive.get('vpp', {}).get('entries', []):
        if not entry['name'].lower().endswith('.v3c'): continue
        with (root / 'Installed_Game' / archive['path']).open('rb') as f:
            f.seek(entry['offset']); data = f.read(entry['size'])
        cursor = 0
        while (cursor := data.find(b'ENOB', cursor)) >= 0:
            start = cursor; cursor += 4
            if start + 12 > len(data): continue
            size, count = struct.unpack_from('<2I', data, start + 4)
            if size != 4 + count * 56 or start + 8 + size > len(data): continue
            payload = data[start + 8:start + 8 + size]
            target = folder / 'payload.bin'; target.write_bytes(payload)
            run = subprocess.run([str(probe), str(target)], capture_output=True, check=True)
            assert run.stdout == payload[4:], entry['name']
            records.append(dict(archive=archive['path'], model=entry['name'], offset=start, count=count))
assert records, 'No candidate BONE sections tested'
miner = next(r for r in records if r['model'].lower() == 'miner.v3c')
assert miner['count'] == 25
report = dict(result='PASS', sections=len(records), bones=sum(r['count'] for r in records),
              scope='Bit-exact raw fields; diagnostic section discovery; no pose conversion', records=records)
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print({k:v for k,v in report.items() if k != 'records'})
