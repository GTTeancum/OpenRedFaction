"""Check per-actor pain lock, attack resumption and an unaffected attack cadence.

Run replay_live_pain.py first. All input is an ordinary recorded replay.
"""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]


def build_input():
    source = (root / 'artifacts/live-pain-replay/input.bin').read_bytes()
    assert source[:8] == b'RFI6' + struct.pack('<I', 48) and len(source) == 8 + 120 * 48
    return source + bytes(120 * 48)


def verify(log):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 240 frames' in log and words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [10, 4, 1]
    assert words('COMBAT_PAIN')[:4] == [3, 3, 2, 2] and words('COMBAT_PAIN')[7] == 0
    gate = words('PAIN_ATTACK_GATE')
    assert gate == [20, 14, 3270, 1717, 1716, 0]
    shots = [list(map(int, l.split()[1:])) for l in log.splitlines() if l.startswith('ENEMY_SHOT_TRACE ')]
    injured = [frame for frame, uid in shots if uid == 3270]
    unaffected = [frame for frame, uid in shots if uid == 3271]
    assert injured == [104, 164, 224] and unaffected == [66, 126, 186]
    assert (injured[0] - 1) * 1000 // 60 < gate[3] <= injured[0] * 1000 // 60
    return dict(result='PASS', frames=240, gate=gate, injured_shots=injured,
        unaffected_shots=unaffected, scope='One injured actor waits until its retained lock expires, resumes on the first eligible frame, and keeps60-frame cadence; another actor fires normally.')


if __name__ == '__main__':
    folder = root / 'artifacts/pain-attack-gate-replay'
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'input.bin'
    source.write_bytes(build_input())
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L4S5.rfl', RF_REPLAY_ITEM_UID='3415', RF_REPLAY_TRACE='1', RF_REPLAY_TRACE_FROM='1')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(source), str(folder / 'frame.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    log = run.stdout + run.stderr
    (folder / 'run.log').write_text(log)
    run.check_returncode()
    report = verify(log)
    (folder / 'report.json').write_text(json.dumps(report, indent=2))
    print(report)
