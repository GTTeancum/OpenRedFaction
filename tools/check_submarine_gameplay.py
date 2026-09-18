"""Bounded process-local sub DEV replay; --run opts into PC execution.

No builds, host input, emulator control or screenshot correctness assertion.
The final image must be inspected separately. Native replay can use inputs.bin.
"""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def replay(exit_vehicle=False):
    rows = []
    for frame in range(300 if exit_vehicle else 240):
        rows.append(struct.pack('<5f7I', 0, 0, float(40 <= frame < 95), 0, 0,
                    0, int(110 <= frame < 125), int(frame == 12 or exit_vehicle and frame == 245),
                    int(frame == 150), 0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--exit', action='store_true', dest='exit_vehicle')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    folder = (args.output_dir or ROOT / 'artifacts' / ('submarine-' + datetime.now().strftime('%Y%m%d-%H%M%S'))).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    inputs = folder / 'inputs.bin'; inputs.write_bytes(replay(args.exit_vehicle))
    env = dict(RF_REPLAY_VEHICLE='sub', RF_REPLAY_DEV_ROOM='1', RF_REPLAY_LEVEL='L5S3.rfl', RF_REPLAY_ARCHIVE='levels1.vpp')
    command = [str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay', str(ROOT / 'Installed_Game'), str(inputs), str(folder / 'final.ppm')]
    report = dict(command=command, env=env, exit=args.exit_vehicle, status='PREPARED')
    if args.run:
        clean = {k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
        with (folder / 'run.log').open('wb') as log:
            result = subprocess.run(command, cwd=ROOT, env=dict(clean, **env), stdout=log, stderr=subprocess.STDOUT, timeout=180)
        if result.returncode: raise RuntimeError(f'PC run failed: {folder / "run.log"}')
        lines = (folder / 'run.log').read_text(errors='replace').splitlines()
        counters = lambda label: list(map(int, next(line for line in lines if line.startswith(label + ' ')).split()[1:]))
        vehicle, weapon = counters('VEHICLE'), counters('SUBMARINE_WEAPON')
        assert vehicle[1:4] == [1, int(args.exit_vehicle), int(not args.exit_vehicle)] and vehicle[5] == 0
        assert weapon[1] == 1 and weapon[2] == 1 and weapon[6:] == [1, 19]
        position = struct.unpack('<3f', struct.pack('<3I', *vehicle[6:9]))
        assert position[2] > 1 and position[1] > -15, 'Expected forward motion and ascent'
        report.update(status='STATE_PASS_VISUALS_REQUIRE_INSPECTION', vehicle=vehicle, weapon=weapon, position=position)
    (folder / 'recipe.json').write_text(json.dumps(report, indent=2) + '\n')
    print(folder)

if __name__ == '__main__':
    main()
