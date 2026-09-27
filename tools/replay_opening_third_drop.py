"""Walk from the third guard to its dropped handgun ammunition."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'guard-8625.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 3040
steps = 305
move = b''.join(struct.pack('<5f7I', .611 if frame < 140 else
                            -.557 if frame < 270 else -.900,
                            0, .791 if frame < 140 else
                            .830 if frame < 270 else .430, 0, 0,
                            0, 0, 0, 0, 0, 0, 0)
                for frame in range(steps))
route = folder / 'third-drop.bin'
route.write_bytes(prefix + move)
env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
env['RF_REPLAY_TRACE'] = '1'
env['RF_REPLAY_TRACE_FROM'] = '3175'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], env=env, capture_output=True, text=True)
(folder / 'third-drop.log').write_text(run.stdout + run.stderr)
run.check_returncode()


def words(label):
    return [int(value) for value in next(line for line in run.stdout.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


body = words('PC_PLAY_BODY')
enemy = words('ENEMY_COMBAT')
report = dict(frames=3040 + steps,
              position=struct.unpack('<3f', struct.pack('<3I', *body[22:25])),
              health=struct.unpack('<f', struct.pack('<I', enemy[5]))[0],
              life=words('PLAYER_LIFE')[:4], shots=enemy[2],
              ammo=words('PLAYER_AMMO')[:3], drops=words('WEAPON_DROPS')[:5],
              enemy_shots=[line for line in run.stdout.splitlines()
                           if line.startswith('ENEMY_SHOT_TRACE ')])
(folder / 'third-drop.json').write_text(json.dumps(report, indent=2))
assert report['life'][0] == 0 and report['health'] > 0
assert report['ammo'][:3] == [3, 16, 4]
assert report['drops'][:4] == [3, 2, 32, 8625]
print(report)
