"""Reproduce a quiet room movement/ammo check; inspect the resulting PPM separately."""
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]


def build_input():
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', -.5 if i < 60 else 0, 0, .5 if i < 60 else 0,
            0, .3 if i < 60 else 0, 0, 0, 0, int(i in (90, 120, 150)), 0, 0, 0)
        for i in range(240))


def verify(log):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    assert 'Completed 240 frames' in log
    assert words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [3, 0, 0]
    assert words('PLAYER_AMMO')[:3] == [3, 125, 13]
    assert words('ENEMY_COMBAT')[1:5] == [0, 0, 0, 0]
    assert not any(l.startswith('NPC_COMBAT_ROW ') for l in log.splitlines())
    position = list(map(float, next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    assert all(abs(a-b) < .002 for a, b in zip(position, [2.308196, -11.118479, 13.218346]))


if __name__ == '__main__':
    folder = root / 'artifacts/dev-room-check'
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'input.bin').write_bytes(build_input())
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TRACE'] = '1'
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--dev-room-replay',
        str(root / 'Installed_Game'), str(folder / 'input.bin'), str(folder / 'frame.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    log = run.stdout + run.stderr
    (folder / 'run.log').write_text(log)
    run.check_returncode()
    verify(log)
    print('PASS: room movement, no NPCs/attacks, three shots and ammo use. Visual/audio review is separate.')
