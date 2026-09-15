"""Verify live NPC pain dispatch and tracking-free short rifle playback."""
import json
import os
from pathlib import Path
import struct
import subprocess
from capture_replay_aim import capture

root = Path(__file__).resolve().parents[1]


def build_input():
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, float(10 <= i < 25), 0, 0, 0, 0, 0, 0, 0,
            int(i == 40), int(60 <= i < 120)) for i in range(120))


def verify(log):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 120 frames' in log and 'COMBAT_PAIN_ERROR' not in log
    assert words('PLAYER_LIFE')[0] == 0 and words('COMBAT')[:3] == [10, 4, 1]
    assert words('COMBAT_PAIN') == [3, 3, 2, 2, 6815847, 1200, 3884216597, 0]
    rows = {int(l.split()[1]): l.split() for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert 56 < float(rows[3270][-1]) < 57 and float(rows[3305][-1]) <= 0
    assert words('NPC_PAIN_AUDIO')[7] == 0
    return dict(result='PASS', frames=120, pain=words('COMBAT_PAIN'),
        scope='Three surviving-hit pain notifications across two actors, two animation starts/sound dispatches, then fatal hit uses death path. No attack-interruption or full campaign claim.')


if __name__ == '__main__':
    folder = root / 'artifacts/live-pain-replay'
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'source.bin'
    source.write_bytes(build_input())
    def run(path, aim):
        env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env.update(RF_REPLAY_LEVEL='L4S5.rfl', RF_REPLAY_ITEM_UID='3415', RF_REPLAY_TRACE='1')
        if aim:
            env['RF_REPLAY_AIM'] = '3305:40:120'
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
    baked = folder / 'input.bin'
    baked.write_bytes(data)
    log = run(baked, False)
    for label in ('COMBAT_PAIN ', 'NPC_PAIN_AUDIO ', 'COMBAT ', 'PC_PLAY_BODY ', 'NPC_COMBAT_ROW '):
        assert [l for l in log.splitlines() if l.startswith(label)] == [l for l in tracked.splitlines() if l.startswith(label)]
    report = verify(log)
    report['look_records'] = count
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
