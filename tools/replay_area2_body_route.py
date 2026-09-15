"""Continue the corrected body-hit medical route without waiting for pursuers.

Run replay_cover_combat.py --medical-crate first. Uses only ordinary replay
input; no actor placement, health grants, forced events or forced level exits.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]


def build_input(exit_route=False):
    prefix = (root / 'artifacts/cover-combat-medical-replay/input.bin').read_bytes()
    assert prefix[:8] == b'RFI6' + struct.pack('<I', 48) and len(prefix) == 8 + 4500 * 48
    # Require the corrected torso aim, not an older oversized-box fixture.
    assert struct.unpack_from('<f', prefix, 8 + 3050 * 48 + 12)[0] > 0
    records = bytearray(prefix[:8 + 4200 * 48])
    for i in range(4200, 4256):
        records.extend(struct.pack('<5f7I', 0, 0, 0, -.2 if i < 4224 else 0,
            .98 if i < 4253 else 0, 0, 0, 0, 0, 0, 0, 0))
    # Reuse the normal northbound movement,644frames earlier than the old fight.
    for i in range(4900, 5350):
        x, z = ((-.974, .226) if i < 4910 else
            (.226, .974) if 4930 <= i < 5130 or 5180 <= i < 5240 else
            (.974, -.226) if 5140 <= i < 5165 else (0, 0))
        records.extend(struct.pack('<5f7I', x, 0, z, 0, 0, 0, 0, 1, 0, 0, 0, 0))
    if exit_route:
        # Historical east-hall/exit movement, now reached644frames earlier.
        tail = (root / 'artifacts/area2-exit-replay/input.bin').read_bytes()
        assert len(tail) == 8 + 7450 * 48 and tail[:8] == prefix[:8]
        records.extend(tail[8 + 5350 * 48:])
    return records


def verify(log, exit_route=False):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    if exit_route:
        assert 'Completed 6806 frames' in log and words('PLAYER_LIFE')[0] == 0
        transitions = [l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]
        assert transitions == [['L2S2a.rfl', 'L2S3.rfl', '5150', '6631']]
        health = struct.unpack('<f', struct.pack('<I', words('ENEMY_COMBAT')[5]))[0]
        assert health == 5 and words('PLAYER_AMMO')[:3] == [3, 120, 16]
        assert 'TAKEN_PICKUP l2s2a.rfl 8553' in log
        return dict(result='PASS', frames=6806, health=health, transitions=transitions,
            scope='Natural L2S2a exit to L2S3 after normal-input medical/corridor route; alive with5health and16loaded rounds. Final combat counters reset on transition; no all-guards-cleared or Xbox claim.')
    assert 'Completed 4706 frames' in log and words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [13, 8, 2]
    health = struct.unpack('<f', struct.pack('<I', words('ENEMY_COMBAT')[5]))[0]
    assert 33 < health < 34 and 'TAKEN_PICKUP l2s2a.rfl 8553' in log
    position = list(map(float, next(l.split()[1:] for l in log.splitlines()
        if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    assert 26.8 < position[0] < 27.3 and -4.3 < position[1] < -4 and 28.4 < position[2] < 28.9
    rows = {int(l.split()[1]): l.split() for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert all(float(rows[uid][-1]) <= 0 for uid in (8490, 5677))
    assert all(float(rows[uid][-1]) > 0 for uid in (5666, 5676, 5678))
    return dict(result='PASS', frames=4706, health=health, position=position,
        scope='Normal-input route reaches northern corridor after two guard kills and medical recovery; three pursuers remain alive. No section-exit or Xbox claim.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit', action='store_true', help='Append the historical east-hall and natural exit route')
    args = parser.parse_args()
    folder = root / ('artifacts/area2-body-route-exit' if args.exit else 'artifacts/area2-body-route')
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'input.bin'
    source.write_bytes(build_input(args.exit))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl', RF_REPLAY_TRACE='1')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / 'frame.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    log = run.stdout + run.stderr
    (folder / 'run.log').write_text(log)
    run.check_returncode()
    report = verify(log, args.exit)
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
