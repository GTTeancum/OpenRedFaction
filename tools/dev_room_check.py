"""Reproduce a quiet room movement/ammo check; inspect the resulting PPM separately."""
import argparse
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]


def build_input(loadout=False):
    if loadout:
        return b'RFI6' + struct.pack('<I',48) + b''.join(
            struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i in (120,155)),0,int(i in (30,60,90)),0)
            for i in range(180))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', -.5 if i < 60 else 0, 0, .5 if i < 60 else 0,
            0, .3 if i < 60 else 0, 0, 0, 0, int(i in (90, 120, 150)), 0, 0, 0)
        for i in range(240))


def verify(log, loadout=False):
    def words(label):
        return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
    if loadout:
        assert 'Completed 180 frames' in log and words('PLAYER_LIFE')[0] == 0
        assert words('WEAPON_SELECTION')[:4] == [3,3,8,1]
        assert words('PLAYER_AMMO')[:3] == [5,48,7]
        assert words('SHOTGUN')[:3] == [1,4,0]
        assert words('ENEMY_COMBAT')[1:5] == [0,0,0,0]
        assert not any(l.startswith('NPC_COMBAT_ROW ') for l in log.splitlines())
        return
    assert 'Completed 240 frames' in log
    assert words('PLAYER_LIFE')[0] == 0
    assert words('COMBAT')[:3] == [3, 0, 0]
    assert words('PLAYER_AMMO')[:3] == [3, 125, 13]
    assert words('ENEMY_COMBAT')[1:5] == [0, 0, 0, 0]
    assert not any(l.startswith('NPC_COMBAT_ROW ') for l in log.splitlines())
    position = list(map(float, next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    assert all(abs(a-b) < .002 for a, b in zip(position, [2.308196, -11.118479, 13.218346]))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loadout', action='store_true')
    args=parser.parse_args()
    folder = root / ('artifacts/dev-room-loadout-check' if args.loadout else 'artifacts/dev-room-check')
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'input.bin').write_bytes(build_input(args.loadout))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TRACE'] = '1'
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--dev-room-replay',
        str(root / 'Installed_Game'), str(folder / 'input.bin'), str(folder / 'frame.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    log = run.stdout + run.stderr
    (folder / 'run.log').write_text(log)
    run.check_returncode()
    verify(log, args.loadout)
    print('PASS: developer room input/state checks. Visual/audio review is separate.')
