"""Walk the real L1S1 route from the charge cut to upper-bridge cover."""

import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'wall-cross.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 4245


def step(strafe, forward):
    return struct.pack('<5f7I', strafe, 0, forward, 0, 0,
                       0, 0, 0, 0, 0, 0, 0)


# This follows the far-side corridor and its winding ramp. The last strafe
# stays north of the bridge guard's opening firing lane.
segments = [(0, 1, 135), (-1, 1, 115), (0, 1, 155), (1, 0, 55),
            (0, 1, 145), (-1, 1, 45), (-1, 0, 60), (0, 1, 40),
            (-1, 0, 162), (-1, 1, 50), (-1, 0, 65),
            (-1, 1, 125), (-1, 0, 55)]
route = folder / 'postwall-bridge-cover.bin'
route.write_bytes(prefix + b''.join(step(x, z) * frames for x, z, frames in segments))
assert (route.stat().st_size - 8) // 48 == 5452

env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], cwd=root, env=env, capture_output=True, text=True)
(folder / 'postwall-bridge-cover.log').write_text(run.stdout + run.stderr)
run.check_returncode()


def values(label):
    return next(line for line in run.stdout.splitlines()
                if line.startswith(label + ' ')).split()[1:]


position = tuple(map(float, values('CAMPAIGN_FINAL_POSITION')))
life = list(map(int, values('PLAYER_LIFE')))
wall = list(map(int, values('CAMPAIGN_WALL')))
assert life[0] == 0 and wall[:2] == [1, 1]
assert 40 < position[0] < 43 and 79 < position[2] < 82, position
print(dict(result='PASS', frames=5452, position=position,
           life=life[:4], wall=wall[:2]))
