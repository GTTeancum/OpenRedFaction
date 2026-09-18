"""Saved rubble lift/stop/retire and release to ordinary gravity, without host input."""
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/moving-rubble-support'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'report.json').unlink(missing_ok=True)
checkpoint = ROOT / 'artifacts/geomod-postedit-re/intermediate-rubble-standing/saved.rfcp'
recording = OUT / 'neutral.bin'
recording.write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(242 * 48))
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_PLAYER_CHECKPOINT='1',
           RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(checkpoint))
report = {}
def f(row, index):
    return struct.unpack('<f', struct.pack('<I', row[index]))[0]

for name in ('stationary', 'moving', 'lifted', 'released'):
    local = dict(env)
    active_recording = recording
    if name != 'stationary':
        local['RF_REPLAY_MOVING_SUPPORT_TEST'] = '1'
    if name == 'released':
        local['RF_REPLAY_MOVING_SUPPORT_TEST'] = '2'
    if name == 'lifted':
        active_recording = OUT / 'lifted.bin'
        active_recording.write_bytes(recording.read_bytes()[:8 + 92 * 48])
    with (OUT / f'{name}.log').open('wb') as log:
        result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'),
                                 '--spawn-replay', str(ROOT / 'Installed_Game'),
                                 str(active_recording), str(OUT / f'{name}.ppm')],
                                cwd=ROOT, env=local, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, (name, result.returncode)
    lines = (OUT / f'{name}.log').read_text().splitlines()
    rows = [list(map(int, line.split()[1:])) for line in lines if line.startswith('MOVING_SUPPORT ')]
    final = next(line for line in lines if line.startswith('CAMPAIGN_FINAL_POSITION '))
    report[name] = dict(rows=rows, final=final)
    if name == 'moving':
        assert len(rows) == 10, rows
        report[name]['decoded'] = [dict(frame=r[0], mode=r[2], support=r[3],
                                        player_y=f(r, 5), body_y=f(r, 8), carry_y=f(r, 11), alive=r[13]) for r in rows]
        print(json.dumps(report[name]['decoded'], indent=2))
        assert all(r[2] == 1 and r[3] and r[13] for r in rows[:6]), 'Lost live support'
        assert abs((f(rows[3], 5) - f(rows[0], 5)) - .25) < .02, 'Player did not follow lift'
        # The first moving contact performs an existing support snap; subsequent
        # displacement must track the actual body translation, not accumulate it twice.
        for r in rows[2:6]:
            assert abs((f(r, 5) - f(rows[1], 5)) - (f(r, 8) - f(rows[1], 8))) < .000002, 'Carry displacement differs'
        assert all(r[4] == rows[0][4] and r[6] == rows[0][6] for r in rows), 'Neutral player drifted sideways'
        baseline_y = float(report['stationary']['final'].split()[2])
        assert abs(baseline_y - f(rows[0], 5)) < .000001, 'Stationary control drifted'
        assert abs(f(rows[5], 5) - f(rows[4], 5)) < .005, 'Player drifted after stop'
        assert f(rows[4], 11) == 0, 'Stopped support retained carry velocity'
        assert all(not r[3] and not r[13] for r in rows[6:]), 'Retired support still attached'
        assert f(rows[-1], 5) < f(rows[5], 5) - .2, 'Player did not fall after retirement'
    if name == 'released':
        assert len(rows) == 10, rows
        decoded = [dict(frame=r[0], mode=r[2], support=r[3], player_y=f(r, 5),
                        body_y=f(r, 8), carry_y=f(r, 11), body_vy=f(r, 15), alive=r[13]) for r in rows]
        report[name]['decoded'] = decoded
        print(json.dumps(decoded, indent=2))
        assert all(r[13] for r in rows), 'Released fragment disappeared'
        assert f(rows[-1], 8) < f(rows[3], 8) - .1, 'Released fragment did not fall'
        assert f(rows[-1], 5) < f(rows[3], 5) - .1, 'Player hovered above falling fragment'
        assert rows[-1][2] == 1, 'Player did not settle'
        assert all(r[2] == 1 and r[3] == rows[0][3] for r in rows), 'Player lost descending support'
        for r in rows[5:7]:
            elapsed = (r[0] - 90) / 60
            assert abs(f(r, 15) + 9.8 * elapsed) < .000002, 'Fragment fall did not follow gravity'
            assert r[11] == r[15], 'Player carry did not refresh from falling body'
        assert rows[-1][11] == rows[-1][15] == 0, 'Settled fragment retained downward carry'
report['scope'] = 'Kinematic lift then either stop/retire or release to ordinary fragment gravity/contact; angular carry remains unqualified.'
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: moving fragment lift, stop, retirement and gravity release')
