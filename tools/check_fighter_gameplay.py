"""Prepare process-local Fighter DEV inputs; --run explicitly runs the PC build.

No builds, emulator launch, host input or desktop capture. State checks do not
establish visual/audio correctness; inspect the native final.ppm separately.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def replay(exit_vehicle=False):
    rows = []
    for frame in range(360 if exit_vehicle else 300):
        rows.append(struct.pack('<5f7I', 0, 0, float(40 <= frame < 80), 0, 0,
                                0, int(90 <= frame < 105),
                                int(frame == 12 or exit_vehicle and frame == 310),
                                int(120 <= frame < 150), 0, 0, int(frame == 160)))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def verify(log, exit_vehicle=False, require_rocket_impact=False):
    lines = log.splitlines()
    def counters(label, count):
        row = next((line for line in reversed(lines) if line.startswith(label + ' ')), None)
        if row is None:
            raise AssertionError('Missing ' + label)
        values = list(map(int, row.split()[1:]))
        if len(values) != count:
            raise AssertionError('Wrong counter count for ' + label)
        return values
    vehicle, weapon = counters('VEHICLE', 16), counters('FIGHTER_WEAPON', 8)
    if vehicle[1:4] != [1, int(exit_vehicle), int(not exit_vehicle)] or vehicle[5] != 0:
        raise AssertionError('Expected one boarding, requested exit state, and no runtime error')
    position = struct.unpack('<3f', struct.pack('<3I', *vehicle[6:9]))
    if not all(math.isfinite(v) for v in position):
        raise AssertionError('Nonfinite vehicle position')
    if math.hypot(position[0] - 30, position[2] + 160) <= .1 or position[1] <= 7.05:
        raise AssertionError('Expected real horizontal flight and ascent from (30,7,-160)')
    # ticks, primary launches, rocket launches, primary contacts, rocket
    # contacts, live flights, primary reserve, rocket reserve.
    if weapon[1] <= 0 or not 0 <= weapon[6] < 900 or weapon[6] != 900 - weapon[1]:
        raise AssertionError('Expected finite minigun launch/ammo debit')
    if weapon[2] != 1 or weapon[7] != 19:
        raise AssertionError('Expected one rocket launch and reserve19')
    if require_rocket_impact and weapon[4] < 1:
        raise AssertionError('Expected requested rocket impact')
    return dict(vehicle=vehicle, weapon=weapon, position=position,
                rocket_impact_observed=weapon[4] > 0,
                rocket_impact_required=require_rocket_impact)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', help='Opt in to running the existing PC executable')
    parser.add_argument('--exit', action='store_true', dest='exit_vehicle', help='Append60 frames with Use310')
    parser.add_argument('--require-rocket-impact', action='store_true', help='Require a proven impact route; default only requires launch/debit')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'artifacts/fighter-gameplay')
    args = parser.parse_args()
    folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    inputs = folder / 'inputs.bin'
    inputs.write_bytes(replay(args.exit_vehicle))
    env = dict(RF_REPLAY_VEHICLE='fighter', RF_REPLAY_DEV_ROOM='1',
               RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp')
    command = [str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
               str(ROOT / 'Installed_Game'), str(inputs), str(folder / 'final.ppm')]
    report = dict(status='PREPARED_NOT_RUN', command=command, cwd=str(ROOT), env=env,
                  frames=360 if args.exit_vehicle else 300, exit=args.exit_vehicle,
                  require_rocket_impact=args.require_rocket_impact,
                  geometry=dict(host=[30, 7, -160], player=[33, 5, -160], player_facing='-X',
                                host_basis='Actual shared Fighter DEV fixture; no replay rotation override'),
                  schedule=['Use12', 'forward40..79', 'rise90..104', 'primary120..149', 'alternate160',
                            'neutral through299'] + (['Use310', 'neutral through359'] if args.exit_vehicle else []),
                  native_options=['--vehicle-test', '--vehicle-class', 'fighter', '--dev-room', '--spawn',
                                  '--level', 'ctf06.rfl', '--archive', 'levelsm.vpp', '--input', str(inputs)],
                  limitations='Rocket impact is conditional until route proven; visuals/audio require separate inspection.')
    recipe = folder / 'recipe.json'
    recipe.write_text(json.dumps(report, indent=2) + '\n')
    if args.run:
        clean = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
        try:
            with (folder / 'run.log').open('wb') as log:
                result = subprocess.run(command, cwd=ROOT, env=dict(clean, **env), stdout=log,
                                        stderr=subprocess.STDOUT, timeout=180)
            if result.returncode:
                raise RuntimeError(f'PC run failed with code{result.returncode}; see {folder / "run.log"}')
            report.update(verify((folder / 'run.log').read_text(errors='replace'), args.exit_vehicle,
                                 args.require_rocket_impact))
            report['status'] = 'STATE_PASS_VISUALS_REQUIRE_INSPECTION'
        except Exception as error:
            report.update(status='FAIL', error=str(error))
            raise
        finally:
            recipe.write_text(json.dumps(report, indent=2) + '\n')
    print(folder)


if __name__ == '__main__':
    main()
