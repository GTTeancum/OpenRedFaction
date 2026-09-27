"""Text-only natural L1S1 charge cut and through-wall traversal check."""

import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'wall-blocker.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 4095
forward = struct.pack('<5f7I', 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)
route = folder / 'wall-cross.bin'
route.write_bytes(prefix + forward * 150)

env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], cwd=root, env=env, capture_output=True,
                     text=True)
(folder / 'wall-cross.log').write_text(run.stdout + run.stderr)
run.check_returncode()


def values(label):
    return next(line for line in run.stdout.splitlines()
                if line.startswith(label + ' ')).split()[1:]


position = tuple(map(float, values('CAMPAIGN_FINAL_POSITION')))
wall = list(map(int, values('CAMPAIGN_WALL')))
life = list(map(int, values('PLAYER_LIFE')))
remote = list(map(int, values('REMOTE')))
assert wall[0:2] == [1, 1] and wall[2] > 500 and wall[3] > 600
assert wall[7] == 0 and life[0] == 0 and remote[0:4] == [1, 1, 1, 1]
assert position[0] > -21.75, position  # beyond the original wall plane
print(dict(result='PASS', frames=4245, position=position, wall=wall,
           life=life[:4], remote=remote[:4]))
