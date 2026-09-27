"""Bake a safe real-spawn route toward L1S1's upper guard."""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
prefix = (folder / 'direct-guard.bin').read_bytes()
assert prefix[:8] == b'RFI6' + struct.pack('<I', 48)
prefix_frames = 2050
records = [struct.pack('<5f7I', 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, int(frame == 0), 0)
           for frame in range(160)]
payload = prefix[:8 + 48 * prefix_frames] + b''.join(records)
source = folder / 'upper-turn-probe.bin'
source.write_bytes(payload)


def replay(path, aim=None):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL'] = 'L1S1.rfl'
    if aim:
        env['RF_REPLAY_AIM'] = aim
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                          str(path)], env=env, capture_output=True, text=True)
    run.check_returncode()
    return run.stdout


aimed = replay(source, '8326:2050:2210')
(folder / 'upper-turn-probe.log').write_text(aimed)
baked = bytearray(payload)
aim_frames = 0
for line in aimed.splitlines():
    if not line.startswith('AIM_INPUT '):
        continue
    _, frame, uid, pitch, yaw = line.split()
    frame = int(frame)
    assert int(uid) == 8326 and 2050 <= frame < 2210
    struct.pack_into('<2f', baked, 8 + frame * 48 + 12,
                     float(pitch), float(yaw))
    aim_frames += 1
assert aim_frames == 160
turn = folder / 'upper-turn.bin'
turn.write_bytes(baked)
turned = replay(turn)
(folder / 'upper-turn.log').write_text(turned)


def words(output, label):
    return [int(value) for value in next(line for line in output.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


for label in ('PLAYER_LIFE', 'PLAYER_AMMO', 'ENEMY_COMBAT', 'PC_PLAY_BODY'):
    assert words(aimed, label) == words(turned, label), label

source = folder / 'upper-walk.bin'
source.write_bytes(baked + b''.join(struct.pack('<5f7I', .609, 0, .793, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0)
                                    for _ in range(90)))
walked = replay(source)
(folder / 'upper-walk.log').write_text(walked)
body = words(walked, 'PC_PLAY_BODY')
position = struct.unpack('<3f', struct.pack('<3I', *body[22:25]))
basis = struct.unpack('<9f', struct.pack('<9I', *body[28:37]))
enemy = words(walked, 'ENEMY_COMBAT')
health = struct.unpack('<f', struct.pack('<I', enemy[5]))[0]
report = dict(frames=2300, position=position, basis=basis, health=health,
              life=words(walked, 'PLAYER_LIFE')[:4], shots=enemy[2],
              ammo=words(walked, 'PLAYER_AMMO')[:3], aim_frames=aim_frames)
(folder / 'upper-walk.json').write_text(json.dumps(report, indent=2))
print(report)
