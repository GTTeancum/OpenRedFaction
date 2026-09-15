"""Rebuild and verify the captured body-collision hall route from local evidence.

Requires the maintenance recording and pain-gate-hall-approach/clearance logs.
This checks input reproduction and completed PC playback, not native coverage.
"""
import json
import math
from pathlib import Path
import struct
from capture_replay_aim import capture
from replay_l2s3_body_entry import compare

root = Path(__file__).resolve().parents[1]


def build_input():
    prefix = (root / 'artifacts/pain-gate-maintenance-baked/input.bin').read_bytes()
    historical = (root / 'artifacts/l3s1-main-room-safe/input.bin').read_bytes()
    assert prefix[:8] == historical[:8] == b'RFI6' + struct.pack('<I', 48)
    assert len(prefix) == 8 + 8402 * 48 and len(historical) >= 8 + 9600 * 48
    yaw = (math.atan2(-.286234, .953973) - math.atan2(-.629723, .483101)) * 60 / 48
    pitch = (math.asin(.089480) - math.asin(-.608328)) * 60 / 48
    turn = b''.join(struct.pack('<5f7I', 0, 0, 0, pitch if i < 48 else 0,
        yaw if i < 48 else 0, 0, 0, 1, 0, 0, 0, 0) for i in range(60))
    approach = prefix + turn + historical[8 + 9000 * 48:8 + 9600 * 48]
    folder = root / 'artifacts/pain-gate-hall-approach'
    assert approach == (folder / 'input.bin').read_bytes()
    recorded, _ = capture(approach, (folder / 'run.log').read_text())
    yaw = math.atan2(.277101, -.960547)
    for i in range(240):
        recorded += struct.pack('<5f7I', math.cos(yaw) if i < 35 else 0, 0,
            math.sin(yaw) if i < 35 else 0, 0, 0, 0, 0, 1,
            int(i in (65, 95, 125, 155, 185)), int(i == 210), 0, 0)
    folder = root / 'artifacts/pain-gate-hall-clearance'
    assert recorded == (folder / 'input.bin').read_bytes()
    return capture(recorded, (folder / 'run.log').read_text())


def verify(tracked, baked):
    compare(tracked, baked)
    for label in ['COMBAT_PAIN ', 'PAIN_ATTACK_GATE ']:
        a = [l for l in tracked.splitlines() if l.startswith(label)]
        assert a and a == [l for l in baked.splitlines() if l.startswith(label)], label
    def words(label):
        return list(map(int, next(l.split()[1:] for l in baked.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 9302 frames' in baked and words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [24, 12, 3]
    health = struct.unpack('<f', struct.pack('<I', words('ENEMY_COMBAT')[5]))[0]
    assert health == 15
    for uid in [2020, 2047, 1751]:
        row = next(l.split() for l in baked.splitlines() if l.startswith(f'NPC_COMBAT_ROW {uid} '))
        assert float(row[-1]) <= 0
    miner = next(l.split() for l in baked.splitlines() if l.startswith('NPC_COMBAT_ROW 2061 '))
    assert float(miner[-1]) == 100
    return dict(result='PASS', frames=9302, health=health, kills=3,
        scope='PC recorded hall combat with body collision and pain locks; native verification is separate.')


if __name__ == '__main__':
    data, count = build_input()
    folder = root / 'artifacts/pain-gate-hall-baked'
    assert data == (folder / 'input.bin').read_bytes()
    report = verify((root / 'artifacts/pain-gate-hall-clearance/run.log').read_text(),
        (folder / 'run.log').read_text())
    report['final_capture_look_records'] = count
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
