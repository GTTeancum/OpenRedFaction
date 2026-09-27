"""Bake a real-spawn handgun route through L1S1 guard UID 8625."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'upper-stairs.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
assert (len(prefix) - 8) // 48 == 2645
start, end = 2645, 3040
records = b''.join(struct.pack('<5f7I', 0, 0, 0, 0, 0,
                               0, 0, 0, int(frame in (30, 75, 120, 165,
                                                     210, 255, 300, 345)),
                               0, 0, 0)
                   for frame in range(end - start))
source = folder / 'guard-8625-aim.bin'
source.write_bytes(prefix + records)


def replay(path, aim=False, trace=False):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
    if aim:
        env['RF_REPLAY_AIM'] = f'8625:{start}:{end}'
    if trace:
        env['RF_REPLAY_TRACE'] = '1'
        env['RF_REPLAY_TRACE_FROM'] = '2640'
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                          str(path)], env=env, capture_output=True, text=True)
    run.check_returncode()
    return run.stdout


aimed = replay(source, aim=True)
baked = bytearray(source.read_bytes())
count = 0
for line in aimed.splitlines():
    if line.startswith('AIM_INPUT '):
        _, frame, uid, pitch, yaw = line.split()
        assert int(uid) == 8625 and start <= int(frame) < end
        struct.pack_into('<2f', baked, 8 + int(frame) * 48 + 12,
                         float(pitch), float(yaw))
        count += 1
assert count == 346
route = folder / 'guard-8625.bin'
route.write_bytes(baked)
output = replay(route, trace=True)
(folder / 'guard-8625.log').write_text(output)


def words(label):
    return [int(value) for value in next(line for line in output.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


for label in ('PLAYER_LIFE', 'PLAYER_AMMO', 'ENEMY_COMBAT', 'PC_PLAY_BODY'):
    expected = [int(value) for value in next(
        line for line in aimed.splitlines() if line.startswith(label + ' ')
    ).split()[1:]]
    assert words(label) == expected, label


guard = next(line for line in output.splitlines()
             if line.startswith('NPC_COMBAT_ROW 8625 ')).split()
enemy = words('ENEMY_COMBAT')
report = dict(frames=end, guard_health=float(guard[-1]),
              health=struct.unpack('<f', struct.pack('<I', enemy[5]))[0],
              life=words('PLAYER_LIFE')[:4], ammo=words('PLAYER_AMMO')[:3],
              shot_rays=[line for line in output.splitlines()
                         if line.startswith('SHOT_RAY ')],
              enemy_shots=[line for line in output.splitlines()
                           if line.startswith('ENEMY_SHOT_TRACE ')],
              aim_frames=count)
(folder / 'guard-8625.json').write_text(json.dumps(report, indent=2))
assert report['guard_health'] <= 0 and report['life'][0] == 0
assert report['ammo'][:3] == [3, 0, 4]
print(report)
