"""Compare all PC replay diagnostics and final pixels with pose sharing on/off."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frames', type=int, default=180)
    p.add_argument('--inputs', type=Path)
    p.add_argument('--actor', type=int, default=9858)
    p.add_argument('--out', type=Path, default=Path('artifacts/pose-sharing'))
    a = p.parse_args()
    if not 1 <= a.frames <= 60000 or not 0 < a.actor < 0xffffffff:
        p.error('Require1..60000 frames and a positive actor UID')
    root = Path(__file__).resolve().parents[1]
    run = a.out.resolve()
    run.mkdir(parents=True, exist_ok=True)
    inputs = a.inputs.resolve() if a.inputs else run / 'inputs.bin'
    if not a.inputs:
        inputs.write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(a.frames * 44))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_ACTOR_UID=str(a.actor))
    outputs, report = {}, {}
    for mode in ('scalar', 'shared'):
        e = dict(env)
        if mode == 'scalar':
            e['RF_REPLAY_NO_POSE_SHARING'] = '1'
        result = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
            str(root / 'Installed_Game'), str(inputs), str(run / (mode + '.ppm'))],
            cwd=root, env=e, capture_output=True, text=True)
        (run / (mode + '.txt')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        outputs[mode] = '\n'.join(x for x in result.stdout.splitlines() if not x.startswith('POSE_SHARING '))
        report[mode] = dict(sha256=hashlib.sha256((run / (mode + '.ppm')).read_bytes()).hexdigest(),
            sharing=next(x for x in result.stdout.splitlines() if x.startswith('POSE_SHARING ')))
    assert outputs['scalar'] == outputs['shared'], 'State diagnostics differ'
    assert report['scalar']['sha256'] == report['shared']['sha256'], 'Final framebuffer differs'
    report.update(result='PASS', scope='L1S1 scalar/shared PC comparison: all reported state/pose diagnostics and final pixels equal.')
    (run / 'comparison.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
