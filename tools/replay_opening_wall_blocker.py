"""Reproduce the first natural L1S1 GeoMod wall with ordinary inputs."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'third-drop.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 3345
records = []


def step(x=0, z=0, yaw=0, jump=0, fire=0, cycle=0):
    records.append(struct.pack('<5f7I', x, 0, z, 0, yaw,
                               0, jump, 0, fire, 0, cycle, 0))


# Walk across the friendly NPC area to the disconnected navigation boundary.
for frame in range(395):
    x, z = ((.793, -.613) if frame < 150 else
            (.932, .363) if frame < 235 else
            (.911, -.414) if frame < 305 else (.998, -.058))
    step(x, z, jump=int(frame == 305))

# Collect Medical Kit UID9789, then return to a safe throw distance.
for _ in range(65):
    step(-.987, .165)
for frame in range(40):
    step(.998, -.058, cycle=2 if frame in (0, 5) else 0)

# Face the solid brush spanning the near side of the route.
for _ in range(80):
    step(yaw=1)

# Plant the charge, retreat, switch to the detonator and fire.
for frame in range(170):
    step(z=-1 if 60 <= frame < 120 else 0,
         fire=int(frame in (0, 130)), cycle=int(frame == 60))

route = folder / 'wall-blocker.bin'
route.write_bytes(prefix + b''.join(records))
assert (route.stat().st_size - 8) // 48 == 4095
env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
env['RF_REPLAY_TRACE'] = '1'
env['RF_REPLAY_TRACE_FROM'] = '3920'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], env=env, capture_output=True, text=True)
(folder / 'wall-blocker.log').write_text(run.stdout + run.stderr)
run.check_returncode()


def words(label):
    return [int(value) for value in next(line for line in run.stdout.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


body = words('PC_PLAY_BODY')
enemy = words('ENEMY_COMBAT')
report = dict(frames=4095,
              position=struct.unpack('<3f', struct.pack('<3I', *body[22:25])),
              health=struct.unpack('<f', struct.pack('<I', enemy[5]))[0],
              life=words('PLAYER_LIFE')[:4], ammo=words('PLAYER_AMMO')[:3],
              selection=words('WEAPON_SELECTION')[:2],
              pickups=words('PICKUPS')[:6],
              taken=[line for line in run.stdout.splitlines()
                     if line.startswith('TAKEN_PICKUP ')],
              remote=words('REMOTE')[:8], geomod=words('GEOMOD')[:8],
              campaign_geomod=words('CAMPAIGN_GEOMOD')[:8],
              campaign_wall=words('CAMPAIGN_WALL')[:8],
              events=[line for line in run.stdout.splitlines()
                      if line.startswith(('REMOTE_ATTACH ', 'REMOTE_DETONATE ',
                                          'GEOMOD_ADMISSION ', 'GEOMOD_HARDNESS '))])
(folder / 'wall-blocker.json').write_text(json.dumps(report, indent=2))
assert report['life'][0] == 0 and report['remote'][:4] == [1, 1, 1, 1]
assert report['campaign_wall'][:2] == [1, 1] and report['campaign_wall'][7] == 0
assert any('9789' in line for line in report['taken'])
print(report)
