"""Compare C streaming section boundaries with independent structural parsing."""
import json
import struct
import subprocess
from pathlib import Path
from inspect_models import inspect
root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
probe = root / 'build/pc/Release/rf_model_file_probe.exe'
records = []; miner = None
for archive in inventory['files']:
    for entry in archive.get('vpp', {}).get('entries', []):
        if not entry['name'].lower().endswith('.v3c'): continue
        path = root / 'Installed_Game' / archive['path']
        with path.open('rb') as f:
            f.seek(entry['offset']); data = f.read(entry['size'])
        parsed = inspect(data)
        expected = [(int(s['type'], 16), s['offset'] + 8, s.get('bytes', 8) - 8) for s in parsed['sections']]
        run = subprocess.run([str(probe), str(path), entry['name']], capture_output=True, check=True)
        actual = [tuple(map(int, line.split())) for line in run.stdout.splitlines()]
        assert actual == expected, entry['name']
        records.append(dict(model=entry['name'], sections=len(actual)))
        if entry['name'].lower() == 'miner.v3c': miner = data, parsed
assert miner
data, parsed = miner
folder = root / 'artifacts/model-file-tests'; folder.mkdir(parents=True, exist_ok=True)
def wrap(payload):
    padded = (len(payload) + 2047) // 2048 * 2048
    archive = bytearray(4096 + padded)
    struct.pack_into('<4I', archive, 0, 0x51890ace, 1, 1, len(archive))
    archive[2048:2058] = b'model.v3c\0'
    struct.pack_into('<I', archive, 2108, len(payload))
    archive[4096:4096 + len(payload)] = payload
    path = folder / 'fixture.vpp'; path.write_bytes(archive)
    return subprocess.run([str(probe), str(path), 'model.v3c'], capture_output=True).returncode
assert wrap(data) == 0
bad = [data[:39], data[:-1], data + b'\0']
offsets = [0, 4, 8, 96, 100, parsed['sections'][0]['lods'][0]['data_offset'] - 4]
for offset in offsets:
    changed = bytearray(data); struct.pack_into('<I', changed, offset, 0xffffffff); bad.append(changed)
for i, payload in enumerate(bad): assert wrap(payload) == 3, i
report = dict(result='PASS', models=len(records), sections=sum(r['sections'] for r in records),
              malformed_cases=len(bad), scope='Structural directory only; no LOD decoding')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
