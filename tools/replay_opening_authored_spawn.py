"""Walk the opening Riot Stick handoff from L1S1's actual player spawn.

Process-local replay only: no staged spawn, injected event, image, or host input.
"""
import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-handoff'
folder.mkdir(exist_ok=True)
source = folder / 'authored-spawn.bin'
frames = 2380

# Adjust the older staged route's strafe component for the installed start
# basis, then enter trigger 9869 through ordinary forward movement.
c, s = .95656, .2914
records = []
for i in range(frames):
    x = z = 0.0
    if 30 <= i < 210:
        z = 1.0
    if 240 <= i < 470:
        x, z = -.8239215, .5667039
    if 500 <= i < 720:
        x, z = .1145, .9934
    if 720 <= i < 772:
        x, z = 0, 1
    records.append(struct.pack('<5f6I', x if i >= 720 else c*x+s*z,
                               0, z, 0, 0, 0, 0, 0, int(i == 2360), 0, 0))
source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(records))
(folder / 'scripted-1800.bin').write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(records[:1800]))

env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(source)], env=env, capture_output=True, text=True)
(folder / 'authored-spawn.log').write_text(run.stdout + run.stderr)
run.check_returncode()

def words(label):
    return [int(v) for v in next(line for line in run.stdout.splitlines()
                                if line.startswith(label + ' ')).split()[1:]]

grants = words('SCRIPT_GRANTS')
ammo = words('PLAYER_AMMO')
contacts = words('TRIGGER_CONTACTS')
life = words('PLAYER_LIFE')
combat = words('COMBAT')
animations = words('SCRIPT_ANIMATION')
looks = words('SCRIPT_LOOK_AT')
slays = words('SCRIPT_SLAYS')
assert grants == [1, 1, 100, 2, 1, 100, 0, 0], grants
assert ammo[:3] == [2, 0, 100] and ammo[7] == 0, ammo
assert contacts[2] == 9869 and contacts[-1] == 0, contacts
assert life[:3] == [0, 0, 0] and life[7] == 0, life
assert combat[0] == 1 and combat[7] == 0, combat
assert animations[:5] == [15, 8, 1, 7, 0] and animations[8:] == [5, 1], animations
assert looks[0] == 4 and looks[2] > 0 and looks[4] == 0 and looks[8] > 0, looks
assert slays[:2] == [2, 2], slays
assert f'Completed {frames} frames' in run.stdout
report = dict(result='PASS', frames=frames, grants=grants, ammo=ammo,
              contact_uid=contacts[2], player_life=life[:3], attacks=combat[0],
              scripted_animations=animations, scripted_looks=looks,
              scripted_slays=slays[:2],
              scope='Real L1S1 player spawn; walking activates opening mission '
                    'triggers, NPC looks/animations and scripted slays, grants Riot Stick '
                    'and permits one attack while alive. No staged pose, '
                    'event injection, image output, or host input. This does not '
                    'establish full mission or interactive controller behavior.')
(folder / 'authored-spawn-report.json').write_text(json.dumps(report, indent=2))
print(report)
