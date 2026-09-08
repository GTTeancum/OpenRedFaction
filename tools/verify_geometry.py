"""Validate C geometry parsing against the independent binary-layout inspector."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
reports = json.loads((root / 'artifacts/geometry.json').read_text())
probe = root / 'build/pc/Release/rf_geometry_probe.exe'
results = []
for report in reports:
    command = [str(probe), str(Path(inventory['root']) / report['archive']), report['file']]
    run = subprocess.run(command, capture_output=True, check=True)
    values = [int(v) for v in run.stdout.split()]
    expected = [report[k] for k in ('textures', 'rooms', 'vertices', 'faces', 'corners', 'mappings', 'bytes')]
    expected += [report['bytes'] + 4 * (report['textures'] + report['rooms'] + report['faces']), report['tail_bytes']]
    assert values == expected, (report['file'], values, expected)
    insufficient = subprocess.run(command + [str(expected[7] - 1)], capture_output=True)
    assert insufficient.returncode == 1 and b'error -4' in insufficient.stderr, report['file']
    results.append(dict(file=report['file'], allocated_bytes=values[7], result='PASS'))
(root / 'artifacts/geometry-verification.json').write_text(json.dumps(results, indent=2))
print(f'PASS: {len(results)} resident geometry loads, accessor bounds, and exact budget rejection')
print('Largest requested allocation:', max(r['allocated_bytes'] for r in results))
