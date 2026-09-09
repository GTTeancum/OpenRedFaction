"""Exercise owned room trees, source binding, budgets and exhaustive ray parity."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'artifacts/inventory.json').read_text())
levels = json.loads((root / 'artifacts/geometry.json').read_text())
results = []
for level in levels:
    run = subprocess.run([str(root / 'build/pc/Release/rf_collision_probe.exe'),
                          '--rooms', str(Path(inventory['root']) / level['archive']),
                          level['file']], capture_output=True)
    assert run.returncode == 0, (level['file'], run.returncode, run.stderr)
    rooms, faces, nodes, peak, queries, hits = map(int, run.stdout.split())
    assert rooms == level['rooms'] and faces == queries == level['faces']
    results.append(dict(file=level['file'], rooms=rooms, faces=faces, nodes=nodes,
                        peak_room_bytes=peak, queries=queries, hits=hits))
report = dict(result='PASS', levels=len(results), faces=sum(r['faces'] for r in results),
              max_room_peak_bytes=max(r['peak_room_bytes'] for r in results),
              scope='PC owned room binding and tree traversal versus exhaustive face queries. '
                    'Exact source vertices, unique level face identities, room membership and '
                    'exact/insufficient budget checks. Not original world room selection or XEMU gameplay.',
              results=results)
(root / 'artifacts/collision-rooms-verification.json').write_text(json.dumps(report, indent=2))
print({k: v for k, v in report.items() if k != 'results'})
