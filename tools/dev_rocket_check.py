"""Ordinary DEV input: launcher supply, delayed excavation and flight while reloading/switching.

No host input. Inspect native/PC frames separately; NPC/occlusion coverage and visible effects remain unfinished.
"""
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def recording(mode):
    end = {'blast-far':260, 'blast-near':300, 'blast-lethal':350}.get(mode, 0)
    frames = end + 100 if end else 125 if mode == 'visual' else 111 if mode == 'flying' else 260
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, .8 if 130 <= i < end else 0, 0, .7 if i < 90 else .2 if mode == 'visual' and i > 110 else 0, 0, 0, 0,
                    int(i == (end+10 if end else 110)), int(mode == 'reload' and i == 120),
                    int(i in (10, 20, 30, 40) or (mode == 'switch' and i == 120)), 0)
        for i in range(frames))

def main():
    folder = ROOT / 'artifacts/rocket-live'
    folder.mkdir(parents=True, exist_ok=True)
    env = {k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TRACE'] = '1'
    for mode in ('flying', 'shot', 'reload', 'switch', 'blast-far', 'blast-near', 'blast-lethal', 'visual'):
        path = folder / (mode + '.bin')
        path.write_bytes(recording(mode))
        run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--dev-room-replay',
            str(ROOT / 'Installed_Game'), str(path), str(folder / (mode + '.ppm'))],
            cwd=ROOT, env=env, capture_output=True, text=True)
        log = run.stdout + run.stderr
        (folder / (mode + '.log')).write_text(log)
        run.check_returncode()
        def row(label):
            return list(map(int, next(l.split()[1:] for l in log.splitlines() if l.startswith(label + ' '))))
        flying = mode in ('flying', 'visual')
        assert row('ROCKETS') == [1, int(not flying), 0, int(flying), int(not flying), 0, 0, 0]
        assert row('GEOMOD')[:3] == [1, int(not flying), 1 + int(not flying)]
        visual = row('ROCKET_VISUAL')
        assert visual[6] == 0
        if mode == 'visual':
            assert visual[1:4] == [1,7,192]
        elif not flying:
            assert visual[1:4] == [0,0,0]
        assert row('COMBAT')[:3] == [1, 0, 0]  # No instantaneous hitscan damage.
        assert row('PLAYER_LIFE')[0] == int(mode == 'blast-lethal')
        blast = row('ROCKET_BLAST')
        assert blast[:5] == [int(not flying), int(not flying), int(mode in ('blast-near','blast-lethal')), 0, int(mode in ('blast-near','blast-lethal'))]
        assert blast[6:] == [0,0]
        health, armor = [struct.unpack('<f', struct.pack('<I', v))[0] for v in row('PICKUP_VITALS')[:2]]
        if mode == 'blast-near':
            assert 80 < health < 100 and 80 < armor < 100
        elif mode == 'blast-lethal':
            assert health <= 0 and armor == 0
        else:
            assert health == 100 and armor == 100
        assert row('WEAPON_SELECTION')[0] == (0 if mode == 'switch' else 4)
        assert row('PLAYER_AMMO')[:3] == ([3,125,16] if mode == 'switch' else [7,17,6] if mode == 'reload' else [7,18,5])
        assert not any(l.startswith('NPC_COMBAT_ROW ') for l in log.splitlines())
        print('PASS:', mode)

if __name__ == '__main__':
    main()
