"""Retained mover rendering after archive, source-level and image closure."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
results = []
for level in json.loads((root / 'artifacts/movers.json').read_text())['results']:
    fields = list(map(int, subprocess.check_output([
        str(root / 'build/pc/Release/rf_scene_check.exe'), '--retained-world',
        str(root / 'Installed_Game' / level['archive']), level['file']
    ]).split()))
    assert len(fields) == 4 and fields[0] == level['count']
    results.append(dict(file=level['file'], movers=fields[0], retained_bytes=fields[1],
                        vertex_capacity_bytes=fields[2], trace_hash=hex(fields[3])))
report = dict(result='PASS', levels=len(results), pose_cases=3*len(results),
              max_retained_bytes=max(row['retained_bytes'] for row in results),
              scope='PC retained scene owner: archive closed, source level poisoned, images closed; '
                    'same allocation at authored and two shifted committed poses, compared to direct world '
                    'projection with independent saved camera. Borrowed world remains loaded. Repeated close. '
                    'Not Xbox runtime lifetime or visible motion.', results=results)
(root / 'artifacts/retained-scene-verification.json').write_text(json.dumps(report, indent=2))
print({key: value for key, value in report.items() if key != 'results'})
