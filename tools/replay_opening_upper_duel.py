"""Bake a moving, process-local aim route through the second L1S1 guard."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'upper-walk.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 2300

start, move_frames, end = 2300, 90, 2550
records = b''.join(struct.pack('<5f7I', .877 if frame < move_frames else 0,
                               0, .480 if frame < move_frames else 0,
                               0, 0, 0, 0, 0,
                               int(frame in (90, 135, 180, 225)),
                               0, 0, 0)
                   for frame in range(end - start))
source = folder / 'upper-duel-aim.bin'
source.write_bytes(prefix + records)


def replay(path, aim=False, trace=False):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
    if aim:
        env['RF_REPLAY_AIM'] = f'8326:{start}:{end}'
    if trace:
        env['RF_REPLAY_TRACE'] = '1'
        env['RF_REPLAY_TRACE_FROM'] = '2380'
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                          str(path)], env=env, capture_output=True, text=True)
    run.check_returncode()
    return run.stdout


aimed = replay(source, aim=True)
(folder / 'upper-duel-aim.log').write_text(aimed)
baked = bytearray(source.read_bytes())
count = 0
for line in aimed.splitlines():
    if line.startswith('AIM_INPUT '):
        _, frame, uid, pitch, yaw = line.split()
        assert int(uid) == 8326 and start <= int(frame) < end
        struct.pack_into('<2f', baked, 8 + int(frame) * 48 + 12,
                         float(pitch), float(yaw))
        count += 1
# The helper stops once the fourth shot kills the tracked guard.
assert count == 226

route = folder / 'upper-duel.bin'
route.write_bytes(baked)
output = replay(route, trace=True)
(folder / 'upper-duel.log').write_text(output)


def words(label):
    return [int(value) for value in next(line for line in output.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


for label in ('PLAYER_LIFE', 'PLAYER_AMMO', 'ENEMY_COMBAT', 'PC_PLAY_BODY'):
    assert words(label) == [int(value) for value in next(
        line for line in aimed.splitlines() if line.startswith(label + ' ')
    ).split()[1:]], label

guard = next(line for line in output.splitlines()
             if line.startswith('NPC_COMBAT_ROW 8326 ')).split()
body = words('PC_PLAY_BODY')
enemy = words('ENEMY_COMBAT')
report = dict(frames=end, guard_health=float(guard[-1]),
              position=struct.unpack('<3f', struct.pack('<3I', *body[22:25])),
              health=struct.unpack('<f', struct.pack('<I', enemy[5]))[0],
              life=words('PLAYER_LIFE')[:4], ammo=words('PLAYER_AMMO')[:3],
              combat=words('COMBAT')[:3],
              shot_rays=[line for line in output.splitlines()
                         if line.startswith('SHOT_RAY ')], aim_frames=count)
(folder / 'upper-duel.json').write_text(json.dumps(report, indent=2))
assert report['guard_health'] <= 0 and report['life'][0] == 0
assert report['ammo'][:3] == [3, 0, 12]
print(report)
