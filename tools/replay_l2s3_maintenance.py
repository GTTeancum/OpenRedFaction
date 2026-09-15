"""Rebuild maintenance look capture and verify ordinary recorded playback.

Requires body-area3-baked and l3s1-main-room-safe local historical fixtures.
--verify-existing checks completed runs without repeating gameplay.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess
from capture_replay_aim import capture
from replay_l2s3_body_entry import compare

root = Path(__file__).resolve().parents[1]


def build_input():
    prefix = (root / 'artifacts/body-area3-baked/input.bin').read_bytes()
    tail = (root / 'artifacts/l3s1-main-room-safe/input.bin').read_bytes()
    assert prefix[:8] == tail[:8] == b'RFI6' + struct.pack('<I', 48)
    assert len(prefix) == 8 + 7356 * 48 and len(tail) >= 8 + 9000 * 48
    yaw = (math.atan2(-.415501, .725412) - math.atan2(-.534670, .820027)) * 60 / 4
    pitch = (math.asin(-.548758) - math.asin(.204165)) * 60 / 48
    turn = b''.join(struct.pack('<5f7I', 0, 0, 0, pitch if i < 48 else 0,
        yaw if i < 4 else 0, 1, 0, 1, 0, 0, 0, 0) for i in range(60))
    return prefix + turn + tail[8 + 8014 * 48:8 + 9000 * 48]


def verify(tracked, baked):
    compare(tracked, baked)
    for label in ['COMBAT_PAIN ', 'PAIN_ATTACK_GATE ']:
        a = [l for l in tracked.splitlines() if l.startswith(label)]
        assert a and a == [l for l in baked.splitlines() if l.startswith(label)], label
    def words(label):
        return list(map(int, next(l.split()[1:] for l in baked.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 8402 frames' in baked and words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [14, 8, 2]
    health = struct.unpack('<f', struct.pack('<I', words('ENEMY_COMBAT')[5]))[0]
    assert health == 15 and len(words('PC_PLAY_BODY')) == 77
    for uid in [2020, 2047]:
        row = next(l.split() for l in baked.splitlines() if l.startswith(f'NPC_COMBAT_ROW {uid} '))
        assert float(row[-1]) <= 0
    miner = next(l.split() for l in baked.splitlines() if l.startswith('NPC_COMBAT_ROW 2061 '))
    assert float(miner[-1]) == 100
    return dict(result='PASS', frames=8402, health=health, kills=2,
        scope='Tracking-free PC maintenance route; native full-route verification remains separate.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-existing', action='store_true')
    args = parser.parse_args()
    folder = root / 'artifacts/pain-gate-maintenance-baked'
    folder.mkdir(parents=True, exist_ok=True)
    source = root / 'artifacts/body-maintenance-track/input.bin'
    tracked_log = root / 'artifacts/pain-gate-maintenance/run.log'
    baked_log = folder / 'run.log'
    data = build_input()
    if args.verify_existing:
        assert source.read_bytes() == data
    else:
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(data)
        def run(path, log_path, tracking):
            env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
            env.update(RF_REPLAY_LEVEL='L2S2a.rfl', RF_REPLAY_TRACE='1')
            if tracking:
                env['RF_REPLAY_AIM'] = '2047:8040:8360'
            result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                str(root / 'Installed_Game'), str(path), str(log_path.with_suffix('.ppm'))],
                cwd=root, env=env, capture_output=True, text=True)
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(result.stdout + result.stderr)
            result.check_returncode()
        run(source, tracked_log, True)
        baked, _ = capture(data, tracked_log.read_text())
        (folder / 'input.bin').write_bytes(baked)
        run(folder / 'input.bin', baked_log, False)
    tracked = tracked_log.read_text()
    baked, count = capture(data, tracked)
    assert (folder / 'input.bin').read_bytes() == baked
    report = verify(tracked, baked_log.read_text())
    report['look_records'] = count
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
