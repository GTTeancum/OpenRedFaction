"""PC-only safe stair approach beyond the defeated second L1S1 guard."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'upper-duel.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 2550
move = b''.join(struct.pack('<5f7I', .582 if frame < 85 else 0,
                            0, .814 if frame < 85 else 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0) for frame in range(95))
route = folder / 'upper-stairs.bin'
route.write_bytes(prefix + move)
env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
env['RF_REPLAY_TRACE'] = '1'
env['RF_REPLAY_TRACE_FROM'] = '2545'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], env=env, capture_output=True, text=True)
(folder / 'upper-stairs.log').write_text(run.stdout + run.stderr)
run.check_returncode()


def words(label):
    return [int(value) for value in next(line for line in run.stdout.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


body = words('PC_PLAY_BODY')
enemy = words('ENEMY_COMBAT')
report = dict(frames=2645,
              position=struct.unpack('<3f', struct.pack('<3I', *body[22:25])),
              health=struct.unpack('<f', struct.pack('<I', enemy[5]))[0],
              life=words('PLAYER_LIFE')[:4], shots=enemy[2],
              ammo=words('PLAYER_AMMO')[:3])
(folder / 'upper-stairs.json').write_text(json.dumps(report, indent=2))
assert report['life'][0] == 0 and report['health'] > 0
print(report)
