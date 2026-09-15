"""Rebuild and verify the body-hit L2S3 guard recording from normal inputs.

Requires the historical area3-entry fixture and corrected medical fixture.
The tracking run records look only; a second run verifies tracking-free playback.
"""
import json
import math
import os
from pathlib import Path
import struct
import subprocess
from capture_replay_aim import capture
from replay_area2_body_route import build_input as area2_input

root = Path(__file__).resolve().parents[1]


def build_input():
    records = bytearray(area2_input(healthy=True))
    tail = (root / 'artifacts/area3-entry-replay/input.bin').read_bytes()
    assert tail[:8] == records[:8] and len(tail) == 8 + 8000 * 48
    records.extend(tail[8 + 7450 * 48:])
    for i in range(6893, 6908):
        struct.pack_into('<f', records, 8 + i * 48 + 16, .98)
    for i in range(6904, 6908):
        struct.pack_into('<f', records, 8 + i * 48 + 12, 0)
    angle = 15 * .98 / 60
    for i in range(6921, 7021):
        x, z = ((.5 * math.cos(angle) - .866 * math.sin(angle),
            .5 * math.sin(angle) + .866 * math.cos(angle)) if i < 7011 else (0, 0))
        struct.pack_into('<f', records, 8 + i * 48, x)
        struct.pack_into('<f', records, 8 + i * 48 + 8, z)
    return records


def verify(log):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 7356 frames' in log and words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [7, 4, 1] and words('PLAYER_AMMO')[:3] == [3, 118, 16]
    assert len(words('PC_PLAY_BODY')) == 77
    health = struct.unpack('<f', struct.pack('<I', words('ENEMY_COMBAT')[5]))[0]
    assert health == 25
    guard = next(l.split() for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW 2020 '))
    assert float(guard[-1]) <= 0
    assert [l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')] == [
        ['L2S2a.rfl', 'L2S3.rfl', '5150', '6631']]
    return dict(result='PASS', frames=7356, health=health, guard_uid=2020)


def compare(tracked, baked):
    labels = ['COMBAT ', 'ENEMY_COMBAT ', 'PLAYER_LIFE ', 'PLAYER_AMMO ',
        'PC_PLAY_BODY ', 'CAMPAIGN_FINAL_POSITION ', 'NPC_COMBAT_ROW ',
        'LEVEL_TRANSITION ', 'COMBAT_EVENT ']
    assert not any(l.startswith('AIM_INPUT ') for l in baked.splitlines())
    for label in labels:
        a = [l for l in tracked.splitlines() if l.startswith(label)]
        b = [l for l in baked.splitlines() if l.startswith(label)]
        assert a and a == b, label


if __name__ == '__main__':
    folder = root / 'artifacts/l2s3-body-entry'
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'source.bin'
    source.write_bytes(build_input())
    def run(path, tracked):
        env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env.update(RF_REPLAY_LEVEL='L2S2a.rfl', RF_REPLAY_TRACE='1')
        if tracked:
            env['RF_REPLAY_AIM'] = '2020:6980:7230'
        result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
            str(root / 'Installed_Game'), str(path), str(path.with_suffix('.ppm'))],
            cwd=root, env=env, capture_output=True, text=True)
        log = result.stdout + result.stderr
        path.with_suffix('.log').write_text(log)
        result.check_returncode()
        verify(log)
        return log
    tracked = run(source, True)
    data, count = capture(source.read_bytes(), tracked)
    baked_path = folder / 'input.bin'
    baked_path.write_bytes(data)
    baked = run(baked_path, False)
    compare(tracked, baked)
    report = verify(baked)
    report.update(look_records=count, scope='Tracking-free PC input reproduction; native verification is separate.')
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
