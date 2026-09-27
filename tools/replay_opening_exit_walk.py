"""Walk the real L1S1 spawn through Riot Stick, first guard, and handgun drop.

The PC-only aim helper is used to record ordinary look inputs once, then an
independent replay checks the baked input file that Xbox can consume unchanged.
"""

import json
import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/opening-exit'
folder.mkdir(exist_ok=True)
prefix = (root / 'artifacts/opening-handoff/authored-spawn.bin').read_bytes()
assert prefix[:8] == b'RFI5' + struct.pack('<I', 44)
prefix_frames = 1805
records = [struct.pack('<5f7I', 0, 0, 1 if frame < 170 else 0, 0, 0,
                       0, 0, 0, 0, 0, 0, int(frame >= 145))
           for frame in range(245)]
records.extend(struct.pack('<5f7I', -.979, 0, -.217, 0, 0,
                           0, 0, 0, 0, 0, int(frame == 0), 0)
               for frame in range(100))
records.extend(struct.pack('<5f7I', -.999, 0, -.034, 0, 0,
                           0, 0, 0, 0, 0, 0, 0) for _ in range(104))
source = folder / 'direct-guard.bin'
prefix_records = (struct.pack('<5f7I', *row, 0) for row in
                  struct.iter_unpack('<5f6I', prefix[8:8 + prefix_frames * 44]))
payload = b'RFI6' + struct.pack('<I', 48) + b''.join(prefix_records) + b''.join(records)
aim_source = folder / 'aim-source.bin'
aim_source.write_bytes(payload)


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


aimed = replay(aim_source, '8324:1805:2050')
(folder / 'aim-source.log').write_text(aimed)
baked = bytearray(payload)
aim_frames = 0
for line in aimed.splitlines():
    if not line.startswith('AIM_INPUT '):
        continue
    _, frame, uid, pitch, yaw = line.split()
    frame = int(frame)
    assert int(uid) == 8324 and 1805 <= frame < 2050
    struct.pack_into('<2f', baked, 8 + frame * 48 + 12,
                     float(pitch), float(yaw))
    aim_frames += 1
assert aim_frames > 100, aim_frames
source.write_bytes(baked)
played = replay(source)
(folder / 'direct-guard.log').write_text(played)


def words(output, label):
    return [int(value) for value in next(line for line in output.splitlines()
                                    if line.startswith(label + ' ')).split()[1:]]


for label in ('SCRIPT_GRANTS', 'RIOT_STICK', 'WEAPON_DROPS', 'PLAYER_AMMO',
              'WEAPON_SELECTION', 'PLAYER_LIFE', 'ENEMY_COMBAT', 'PC_PLAY_BODY'):
    assert words(played, label) == words(aimed, label), label


body = words(played, 'PC_PLAY_BODY')
at = struct.unpack('<3f', struct.pack('<3I', *body[22:25]))
enemy = words(played, 'ENEMY_COMBAT')
health = struct.unpack('<f', struct.pack('<I', enemy[5]))[0]
report = dict(frames=prefix_frames + len(records), position=at, health=health,
              life=words(played, 'PLAYER_LIFE')[:4],
              grants=words(played, 'SCRIPT_GRANTS')[:3],
              riot=words(played, 'RIOT_STICK')[:6],
              weapon=words(played, 'PLAYER_AMMO')[:3],
              selection=words(played, 'WEAPON_SELECTION')[:2],
              drops=words(played, 'WEAPON_DROPS')[:5], aim_frames=aim_frames,
              enemy_shots=enemy[2], transitions=[line for line in played.splitlines()
                                                 if line.startswith('LEVEL_TRANSITION ')])
assert report['life'] == [0, 0, 0, 0] and report['grants'] == [1, 1, 100]
assert report['drops'][:4] == [1, 1, 16, 8324] and report['weapon'] == [3, 0, 16]
assert 0 < health <= 100 and abs(at[0] + 53.52) < 1 and abs(at[2] - 13.74) < 1
assert not report['transitions']
(folder / 'direct-guard-report.json').write_text(json.dumps(report, indent=2))
print(report)
