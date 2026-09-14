"""Compare complete native framebuffers from matching completed replay fixtures."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops

p = argparse.ArgumentParser()
p.add_argument('baseline', type=Path)
p.add_argument('candidate', type=Path)
a = p.parse_args()
reports = [json.loads((d / 'report.json').read_text()) for d in (a.baseline, a.candidate)]
for report in reports:
    assert report['result'] == 'PASS' and report.get('capture'), 'Requires completed native captures'
for key in ('level', 'final_level', 'archive', 'frames', 'input_sha256'):
    assert reports[0][key] == reports[1][key], (key, 'Replay fixtures differ')
images = [Image.open(d / 'framebuffer.png').convert('RGB') for d in (a.baseline, a.candidate)]
assert images[0].size == images[1].size, 'Framebuffer dimensions differ'
difference = ImageChops.difference(*images)
bounds = difference.getbbox()
result = dict(result='PASS' if bounds is None else 'FAIL',
              baseline=str(a.baseline.resolve()), candidate=str(a.candidate.resolve()),
              dimensions=images[0].size, differing_bounds=bounds,
              rgb_sha256=[hashlib.sha256(i.tobytes()).hexdigest() for i in images],
              scope='Exact complete final native framebuffer comparison for this replay; no claim about other frames or campaign sections.')
(a.candidate / 'native-image-comparison.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
if bounds is not None:
    difference.save(a.candidate / 'native-image-difference.png')
    raise SystemExit(1)
