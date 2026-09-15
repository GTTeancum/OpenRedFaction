"""Compare ordinary movement into an intact wall and a developer excavation.

Only game-local replay input is used. Inspect the resulting frames separately.
"""
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def recording(cut, frames=400):
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, .8 if i >= 130 else 0,
                    0, .7 if i < 90 else 0, 0, 0,
                    int(cut and i == 110), 0, 0, 0,
                    int(cut and i == 110))
        for i in range(frames))


def main():
    folder = ROOT / 'artifacts/geomod-live'
    folder.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_TRACE'] = '1'
    positions = []
    for cut in (False, True):
        name = 'walk-cut' if cut else 'walk-original'
        path = folder / (name + '.bin')
        path.write_bytes(recording(cut))
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
        assert all(abs(a-b) < .003 for a, b in zip(position, expected)), position
        assert stats[:3] == [1, int(cut), 1 + int(cut)], stats
        assert stats[5:] == [0, int(cut), int(cut)], stats
        assert stats[3] <= stats[4] <= 1024*1024 + 65536, stats
        assert int(row('PLAYER_LIFE')[0]) == 0
        assert 'Completed 400 frames' in log
        positions.append(position)
    assert positions[1][0] < -16 and positions[0][0] > -16
    print('PASS: live developer excavation and body movement beyond the original wall; visual/audio review is separate.')


if __name__ == '__main__':
    main()
