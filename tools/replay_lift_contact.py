"""Stage on the authored L1S2 lift; report contact and activation separately.

The use pulse is delivered through the replay input adapter and authored trigger.
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cycle', action='store_true', help='Include automatic return and stopped dwell')
args = parser.parse_args()
count = 600 if args.cycle else 360
folder = root / ('artifacts/lift-cycle' if args.cycle else 'artifacts/lift-contact')
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI3' + struct.pack('<I', 32) + b''.join(
    struct.pack('<5f3I', 0, 0, 0, 0, 0, 0, 0, frame == 30) for frame in range(count)))
env = dict(os.environ)
for key in ('RF_REPLAY_REGION_START', 'RF_REPLAY_DOOR_START'):
    env.pop(key, None)
env.update(RF_REPLAY_LEVEL='L1S2.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_LIFT_START='1')
exe = root / 'build/pc/Release/rf_pc_play.exe'
run = subprocess.run([str(exe), '--spawn-replay', str(root / 'Installed_Game'),
                      str(source), str(folder / 'frame.ppm')], cwd=root, env=env,
                     capture_output=True, text=True, check=True)
(folder / 'pc.txt').write_text(run.stdout + run.stderr)


def row(name):
    return list(map(int, next(line for line in run.stdout.splitlines()
                             if line.startswith(name + ' ')).split()[1:]))


motion, activation, contact = row('LIVE_MOTION'), row('LIVE_ACTIVATION'), row('BODY_CONTACT')
body = row('PC_PLAY_BODY')
assert motion[7] == 0 and row('BODY_SWEEPS')[3] == 0
movers = next(level for level in json.loads((root / 'artifacts/movers.json').read_text())['results']
              if level['file'] == 'L1S2.rfl')['records']
assert contact[17] < len(movers), 'Final sweep did not contact a mover'
uid = movers[contact[17]]['uid']
assert uid == 8670, ('Unexpected contacted mover', uid)
position = struct.unpack('<3f', struct.pack('<3I', *body[22:25]))
report = dict(result='PASS', frames=count, cycle=args.cycle, staged=True,
              level='L1S2.rfl', mover_uid=uid, player_position=position,
              motion=motion, activation=activation,
              pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              scope='Staged authored L1S2 lift with one use pulse at frame30. '
                    'Checkpoint body positions verify rising travel and optional return. '
                    'No every-frame continuity, route from authored spawn or Xbox runtime claim.')
assert activation[2] == 1 and motion[4] == (3 if args.cycle else 1), (activation, motion)
checkpoints = []
for frames in ((30, 60, 90, 120, 150, 180, 360, 420, 480, 540) if args.cycle else (30, 60, 90, 120, 150, 180)):
    checkpoint = folder / 'checkpoint-input.bin'
    checkpoint.write_bytes(source.read_bytes()[:8 + frames * 32])
    replay = subprocess.run([str(exe), '--spawn-replay', str(root / 'Installed_Game'),
                             str(checkpoint), str(folder / 'checkpoint.ppm')],
                            cwd=root, env=env, capture_output=True, text=True, check=True)
    words = list(map(int, next(line for line in replay.stdout.splitlines()
                              if line.startswith('PC_PLAY_BODY ')).split()[1:]))
    xyz = struct.unpack('<3f', struct.pack('<3I', *words[22:25]))
    checkpoints.append(dict(frames=frames, position=xyz))
assert abs(checkpoints[5]['position'][1] - checkpoints[0]['position'][1] - 2.5) < 0.0001
if args.cycle:
    assert abs(position[1] - checkpoints[0]['position'][1]) < 0.0001
    assert abs(checkpoints[6]['position'][1] - checkpoints[5]['position'][1]) < 0.0001
    assert checkpoints[6]['position'][1] > checkpoints[7]['position'][1] > checkpoints[8]['position'][1] > checkpoints[9]['position'][1]
    assert abs(position[1] - checkpoints[9]['position'][1]) < 0.0001
    assert struct.unpack('<3f', struct.pack('<3I', *body[46:49])) == (0., 0., 0.)
assert all(b['position'][1] >= a['position'][1] for a, b in zip(checkpoints[:6], checkpoints[1:6]))
assert all(abs(c['position'][0] - position[0]) < 0.0001 and
           abs(c['position'][2] - position[2]) < 0.0001 for c in checkpoints)
assert checkpoints[1]['position'][1] < checkpoints[2]['position'][1] < checkpoints[3]['position'][1]
report['checkpoints'] = checkpoints
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
