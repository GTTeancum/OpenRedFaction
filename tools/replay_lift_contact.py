"""Stage on the authored L1S2 lift; report contact and activation separately.

This currently records the missing use-input connection, not successful carry.
"""
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/lift-contact'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(struct.pack('<5fI', 0, 0, 0, 0, 0, 0) * 360)
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
report = dict(result='CONTACT_VERIFIED_CARRY_UNVERIFIED', frames=360, staged=True,
              level='L1S2.rfl', mover_uid=uid, player_position=position,
              motion=motion, activation=activation,
              pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              scope='Authored lift mover contacted from an explicit staged start. '
                    'No input or activation forced. Trigger8689 flag1 requires '
                    'use input, currently absent from campaign contact dispatch. '
                    'No sustained carry, authored route or Xbox runtime claim.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
