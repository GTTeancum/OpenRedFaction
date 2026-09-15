"""Compare ordinary movement into an intact wall and a developer excavation.

Only game-local replay input is used. Inspect the resulting frames separately.
"""
import os
import argparse
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def recording(cut, frames=400, mode='single'):
    def pressed(i):
        if not cut:
            return False
        if mode == 'held':
            return i >= 110
        return i in ((110, 400, 500, 600) if mode == 'repeat' else (110,))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, .8 if i >= 130 else 0,
                    0, .7 if i < 90 else 0, 0, 0,
                    int(pressed(i)), 0, 0, 0, int(pressed(i)))
        for i in range(frames))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extended', action='store_true', help='Also check held input and repeated live cuts')
    args = parser.parse_args()
    folder = ROOT / 'artifacts/geomod-live'
    folder.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TRACE'] = '1'
    positions = []
    cases = [('walk-original', False, 400, 'single'), ('walk-cut', True, 400, 'single')]
    if args.extended:
        cases += [('held', True, 400, 'held'), ('repeat', True, 720, 'repeat')]
    for name, cut, frames, mode in cases:
        path = folder / (name + '.bin')
        path.write_bytes(recording(cut, frames, mode))
        run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'),
            '--dev-room-replay', str(ROOT / 'Installed_Game'), str(path),
            str(folder / (name + '.ppm'))], cwd=ROOT, env=env,
            capture_output=True, text=True)
        log = run.stdout + run.stderr
        (folder / (name + '.log')).write_text(log)
        run.check_returncode()
        def row(label):
            return next(line.split()[1:] for line in log.splitlines()
                        if line.startswith(label + ' '))
        stats = list(map(int, row('GEOMOD')))
        position = list(map(float, row('CAMPAIGN_FINAL_POSITION')))
        expected = [-17.382938, -11.951555, 5.566381] if cut else [-15.388512, -11.118479, 5.927131]
        edits = 4 if mode == 'repeat' else int(cut)
        if mode == 'repeat':
            expected = [-23.391994, -14.450785, .282855]
        assert all(abs(a-b) < .003 for a, b in zip(position, expected)), position
        assert stats[:3] == [1, edits, 1 + edits], stats
        assert stats[5:] == [0, edits, edits], stats
        assert stats[3] <= stats[4] <= 1024*1024 + 65536, stats
        assert int(row('PLAYER_LIFE')[0]) == 0
        assert f'Completed {frames} frames' in log
        positions.append(position)
    assert positions[1][0] < -16 and positions[0][0] > -16
    print('PASS: live developer excavation and body movement beyond the original wall; visual/audio review is separate.')


if __name__ == '__main__':
    main()
