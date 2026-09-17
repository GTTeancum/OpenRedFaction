"""Compare ordinary player movement against intact and excavated Glass House walls.

Uses a fixed-aim sixteen-cut cavity checkpoint and process-local replay only.
Native validation uses the generated input with xemu_render_check separately.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--round-trip', action='store_true', help='Back out after settling inside, then settle in the room')
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/pc-expanded')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'artifacts/geomod-cavity-walk')
    args = parser.parse_args()
    folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    checkpoint = args.checkpoint.resolve()
    exe = args.build_dir.resolve()/'Release/rf_pc_play.exe'
    frames = 930 if args.round_trip else 510
    payload = b'RFI6'+struct.pack('<I', 48)+b''.join(
        struct.pack('<5f7I', 0, 0, int(90 <= i < 450)-int(args.round_trip and 510 <= i < 870), 0, .7 if i < 90 else 0, *([0]*7))
        for i in range(frames))
    (folder/'input.bin').write_bytes(payload)
    report = dict(result='FAIL', frames=frames, round_trip=args.round_trip, cases={}, input_sha256=hashlib.sha256(payload).hexdigest(),
                  checkpoint_sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
                  binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  scope='Glass House fixed-aim wall: ordinary walk and settle, intact-wall control versus restored excavation. No native or visual acceptance from this script.')
    try:
        traces = {}
        for name in ('uncut', 'cut'):
            env = {k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
            env.update(RF_REPLAY_TRACE='1', RF_REPLAY_TRACE_FROM='0')
            if name == 'cut':
                env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(checkpoint)
            with (folder/(name+'.log')).open('wb') as log:
                result = subprocess.run([str(exe), '--dev-room-replay', str(ROOT/'Installed_Game'),
                    str(folder/'input.bin'), str(folder/(name+'.ppm'))], cwd=ROOT, env=env,
                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
            assert result.returncode == 0, name+' replay failed'
            lines = (folder/(name+'.log')).read_text().splitlines()
            words = list(map(int, next(l for l in lines if l.startswith('PC_PLAY_BODY ')).split()[1:]))
            position = struct.unpack('<3f', struct.pack('<3I', *words[22:25]))
            traces[name] = {int(p[1]):tuple(map(float, p[2:5])) for p in
                            (l.split() for l in lines if l.startswith('CAMPAIGN_TRACE '))}
            assert set(traces[name]) == set(range(1, frames)), 'Incomplete post-initialization movement trace'
            assert max(sum((v[k]-position[k])**2 for k in range(3))**.5
                       for frame,v in traces[name].items() if frame >= frames-30) < .01, name+' did not settle'
            report['cases'][name] = dict(position=position, body_words=words,
                samples={str(frame):traces[name][frame] for frame in ((90, 180, 270, 360, 450, 509, 600, 750, 870, 929) if args.round_trip else (90, 180, 270, 360, 450, 509))})
        assert all(traces['cut'][i] == traces['uncut'][i] for i in range(1, 271)), 'Paths differ before wall contact'
        assert -16 < traces['uncut'][509][0] < -15, 'Intact wall did not block player'
        assert -40 < traces['cut'][509][0] < -20, 'Player did not enter excavation'
        assert all(-16 < p[1] < -9 and -20 < p[2] < 20 for p in traces['cut'].values()), 'Unexpected fall or escape'
        report['first_wall_crossing_frame'] = next(i for i,p in traces['cut'].items() if p[0] < -16)
        if args.round_trip:
            assert traces['cut'][frames-1][0] > -15, 'Player did not leave excavation'
            assert abs(traces['cut'][frames-1][1]-traces['uncut'][frames-1][1]) < 1e-4, 'Player did not regain room-floor height'
            report['return_wall_crossing_frame'] = next(i for i,p in traces['cut'].items() if i>510 and p[0]>-16)
        report['result'] = 'PASS'
    finally:
        (folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
