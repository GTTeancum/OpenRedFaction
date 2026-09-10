"""Check integrated campaign initialization against original selector/pose fixtures."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
for tool in ('inspect_initial_player_motion.py', 'verify_eye_setup.py'):
    subprocess.run([sys.executable, str(root/'tools'/tool)], cwd=root, check=True, stdout=subprocess.DEVNULL)
folder = root/'artifacts/campaign-initialization'
folder.mkdir(exist_ok=True)
source = folder/'neutral.bin'
source.write_bytes(bytes(24*120))
pc = root/'build/pc/Release/rf_pc_play.exe'
run = subprocess.run([str(pc), '--spawn-replay', str(root/'Installed_Game'), str(source), str(folder/'final.ppm')],
                     cwd=root, capture_output=True, text=True, check=True)
(folder/'pc.txt').write_text(run.stdout)
rows = {line.split()[0]: list(map(int, line.split()[1:])) for line in run.stdout.splitlines() if line.startswith('PLAYER_')}
word = lambda value: struct.unpack('<I', struct.pack('<f', value))[0]
f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
motion = json.loads((root/'artifacts/initial-player-motion.json').read_text())
case = next(c for c in motion['results'] if c['player_flag'] == 1 and c['primary'] == 5 and c['secondary'] == -1)
initial = rows['PLAYER_INITIAL_ANIMATION']
assert initial[:2] == [0, 1]
assert initial[2:6] == [0, 1, word(case['controller'][2]), word(case['controller'][3])]
assert initial[9] == len(case['slots']) and initial[11] == word(case['slots'][0][2])
poses = json.loads((root/'artifacts/eye-setup-verification.json').read_text())
assert motion['result'] == poses['result'] == 'PASS'
poses = [p for p in poses['results'] if p['initial_phase'] == 0 and p['initial_elapsed'] == f32(1/30)]
standing = next(p for p in poses if p['query'] == 'standing')
crouching = next(p for p in poses if p['query'] == 'crouching')
assert rows['PLAYER_CLASS_EYE'] == [0, word(standing['eye'][1]), 0, 0, word(crouching['eye'][1]), 0]
count = len(standing['spheres'])
assert count == len(crouching['spheres']) and count <= 8
centers = []
for pose in (standing, crouching):
    for sphere in pose['spheres']:
        centers.extend([0, word(sphere[1]), 0])  # miner1 class axis restriction
    centers.extend([0]*((8-count)*3))
height = max(0, *(f32(a[1]-b[1]) for a, b in zip(standing['spheres'], crouching['spheres'])))
assert rows['PLAYER_CLASS_STANCE'] == [count]+centers+[word(height)]
report = dict(result='PASS', frames=120, pc_sha256=hashlib.sha256(pc.read_bytes()).hexdigest(),
              initial_animation=initial, eye=rows['PLAYER_CLASS_EYE'], stance=rows['PLAYER_CLASS_STANCE'],
              scope='Integrated initial controller/first-slot weight and class centers/eye match original selector and loaded neutral NPC pose fixtures. Does not execute a full original factory, weapon ownership, player contact response or full campaign.')
(folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
