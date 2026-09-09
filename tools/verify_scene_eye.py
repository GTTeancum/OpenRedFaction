"""Compare integrated diagnostic eye offsets with original loaded-pose evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--scene-log', type=Path, help='Use an existing PC --follow log instead of rerunning it')
parser.add_argument('--original-report', type=Path, help='Use an existing verify_eye_setup report instead of rerunning it')
args = parser.parse_args()
reference_path = args.original_report or root/'artifacts/eye-setup-verification.json'
if args.original_report is None:
    subprocess.run([sys.executable, str(root/'tools/verify_eye_setup.py')], cwd=root, check=True, stdout=subprocess.DEVNULL)
scene_path = args.scene_log or root/'artifacts/scene-eye-follow.txt'
if args.scene_log is None:
    command = [str(root/'build/pc/Release/rf_scene_check.exe'), str(root/'Installed_Game/levels1.vpp'), 'L1S1.rfl', '9858']
    command += [str(root/'Installed_Game'/name) for name in ('meshes.vpp','motions.vpp','tables.vpp','maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')]
    with scene_path.open('w') as output:
        subprocess.run(command+['--follow'], cwd=root, check=True, stdout=output)
reference = json.loads(reference_path.read_text())
assert reference['result'] == 'PASS'
rows = [v for v in reference['results'] if v['initial_phase'] == 0 and v['initial_elapsed'] == struct.unpack('<f', struct.pack('<f', 1/30))[0]]
expected = []
for query in ('standing', 'crouching'):
    eye = next(v['eye'] for v in rows if v['query'] == query)
    expected.extend((0, eye[1], 0))  # miner1 class flag 20000, verified 40a150/423bd0.
line = next(line for line in scene_path.read_text().splitlines() if line.startswith('ACTOR_EYE_OFFSETS '))
words = list(map(int, line.split()[1:]))
assert struct.pack('<6I', *words) == struct.pack('<6f', *expected), (words, expected)
report = dict(result='PASS', values=expected, words=words,
              scene_log_sha256=hashlib.sha256(scene_path.read_bytes()).hexdigest(),
              original_report_sha256=hashlib.sha256(reference_path.read_bytes()).hexdigest(),
              reused_scene=args.scene_log is not None, reused_original=args.original_report is not None,
              scope='Six integrated PC model-space offsets match original loaded stand/crouch sequence after miner1 axis restriction; no proof of full spawn execution or camera binding.')
(root/'artifacts/scene-eye-verification.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
