"""Process-local jump replays on the rising and falling authored L1S2 lift."""
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/lift-jump'
folder.mkdir(exist_ok=True)
env = dict(os.environ)
for key in ('RF_REPLAY_REGION_START', 'RF_REPLAY_DOOR_START'):
    env.pop(key, None)
env.update(RF_REPLAY_LEVEL='L1S2.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_LIFT_START='1')
exe = root / 'build/pc/Release/rf_pc_play.exe'
real = lambda word: struct.unpack('<f', struct.pack('<I', word))[0]
results = []
for count, jump, floor in ((210, 90, -3.555978775024414), (540, 430, -6.055978775024414)):
    source = folder / f'inputs-{jump}.bin'
    source.write_bytes(b'RFI3' + struct.pack('<I', 32) + b''.join(
        struct.pack('<5f3I', 0, 0, 0, 0, 0, 0, i == jump, i == 30) for i in range(count)))
    run = subprocess.run([str(exe), '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / f'frame-{jump}.ppm')], cwd=root,
                         env=env, capture_output=True, text=True, check=True)
    (folder / f'pc-{jump}.txt').write_text(run.stdout + run.stderr)
    def row(name):
        return list(map(int, next(line for line in run.stdout.splitlines()
                                 if line.startswith(name + ' ')).split()[1:]))
    assert row('PLAYER_JUMP') == [1, 1, 1, jump]
    raw = row('PLAYER_JUMP_FRAMES')
    ring = {raw[i]: raw[i:i + 8] for i in range(0, len(raw), 8)}
    assert ring[jump - 1][4] == 1
    assert ring[jump][1:5] == [1, 1, 1, 3]
    landing = next(i for i in range(jump + 1, count) if ring[i][4] == 1)
    assert 40 < landing - jump < 100, landing
    for i in range(jump + 1, count):
        assert ring[i][1:4] == [0, 0, 0], ('Unexpected repeat', i)
        assert ring[i][4] == (3 if i < landing else 1), ('Mode continuity', i)
    for i in range(jump + 1, landing):
        assert real(ring[i][6]) < real(ring[i - 1][6]), ('Airborne velocity', i)
    assert any(real(ring[i][6]) < 0 for i in range(jump, landing))
    for i in range(landing, count):
        assert real(ring[i][6]) == 0, ('Grounded vertical velocity', i)
    body = row('PC_PLAY_BODY')
    position = [real(value) for value in body[22:25]]
    assert abs(position[1] - floor) < 0.0001
    assert abs(position[0] - 83.81486511230469) < 0.0001
    assert abs(position[2] + 44.49818420410156) < 0.0001
    assert row('LIVE_MOTION')[7] == row('BODY_SWEEPS')[3] == 0
    results.append(dict(frames=count, jump_frame=jump, landing_frame=landing,
                        final_position=position, input_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
report = dict(result='PASS', cases=results, pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              scope='Staged authored lift, one use pulse and one jump pulse per replay. '
                    'Every retained frame from takeoff through final stop checks mode, '
                    'no repeat and vertical velocity; final position returns to lift. '
                    'No horizontal dismount, original full-arc equivalence, per-frame '
                    'support identity or native Xbox execution claim in this report.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
