"""Cross the first L1S2 low-water gap from the natural campaign arrival save.

All controls are baked process-local input; no staged pose, event or host input.
"""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
source = folder / 'L1S2-arrival-crossing-860.bin'
header = b'RFI6' + struct.pack('<I', 48)


def step(strafe, forward, jump=0):
    return struct.pack('<5f7I', strafe, 0, forward, 0, 0,
                       0, jump, 0, 0, 0, 0, 0)


source.write_bytes(header + step(-1, 0) * 220 +
                   step(-.707, .707) * 400 + step(-1, 0, 1) * 240)
assert (source.stat().st_size - 8) // 48 == 860
env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L1S2.rfl', RF_REPLAY_ARCHIVE='levels1.vpp',
           RF_REPLAY_WORLD_SNAPSHOT_IN=str(folder / 'L1S2-arrival-save'),
           RF_REPLAY_WORLD_SNAPSHOT_OUT=str(folder / 'L1S2-crossing-shore-save'),
           RF_REPLAY_TRACE='1')
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(source)], cwd=root, env=env, capture_output=True, text=True)
output = run.stdout + run.stderr
(folder / 'L1S2-arrival-crossing-860.log').write_text(output)
run.check_returncode()


def row(label):
    return next(line.split()[1:] for line in output.splitlines()
                if line.startswith(label + ' '))


position = tuple(map(float, row('CAMPAIGN_FINAL_POSITION')))
life = list(map(int, row('PLAYER_LIFE')))
target_health = float(next(line.split()[-1] for line in output.splitlines()
                           if line.startswith('NPC_COMBAT_ROW 9709 ')))
assert life[0] == 0 and target_health <= 0 and not any(
    line.startswith('LEVEL_TRANSITION ') for line in output.splitlines())
assert 11 < position[0] < 15 and -11 < position[1] < -9 and -87 < position[2] < -83
assert 'WORLD_SNAPSHOT_STORED bytes' in output
report = dict(result='PASS', frames=860, position=position, player_alive=True,
              target_9709_health=target_health,
              scope='Natural L1S1-to-L1S2 arrival save followed by ordinary '
                    'text-only PC movement and a fresh far-side checkpoint; '
                    'Xbox validation is separate and visual content is unverified.')
(folder / 'L1S2-arrival-crossing-860.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
