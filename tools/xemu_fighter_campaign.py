"""Bounded stock-64-MiB Xbox check of the authored L13S3 Fighter01 host.

The process-local actor fixture supplies input only inside the guest game.
No PC gameplay, campaign route, screen capture or host input is involved.
"""
import datetime
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FRAMES = 80
UID = 8955
POSITION = (24.44294548034668, -26.2889404296875, 20.15338897705078)
ROUTE_TARGET = (-82.29115295410156, -49.269466400146484, 110.44220733642578)


def replay(board, frames):
    rows = []
    for frame in range(frames):
        rows.append(struct.pack('<5f7I', 0, 0, 0, 0, 0,
                                0, 0, int(board and frame == 12),
                                int(board and 40 <= frame < 55),
                                0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--route', action='store_true',
                        help='Leave the authored fighter unoccupied and check its auto waypoint route')
    args = parser.parse_args()
    frames = 180 if args.route else FRAMES
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('fighter-campaign-l13s3-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox L13S3 authored Fighter01',
              'uid': UID, 'route_fixture': args.route}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels3.vpp'.ljust(64, b'\0') + b'L13S3.rfl'.ljust(64, b'\0'))
        if not args.route:
            (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', UID))
        else:
            (DISC / 'campaign-trigger-start.bin').write_bytes(struct.pack('<I', 9820))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(not args.route, frames))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, frames, 180, snapshot=True,
                          extra_symbols={'rf_scene_vehicle_enabled': 1,
                                         'rf_scene_vehicle_state': 16,
                                         'rf_scene_vehicle_route_state': 8,
                                         'rf_scene_fighter_weapon': 8},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        enabled = guest['extra']['rf_scene_vehicle_enabled'][0]
        vehicle = guest['extra']['rf_scene_vehicle_state']
        weapon = guest['extra']['rf_scene_fighter_weapon']
        position = [struct.unpack('<f', struct.pack('<I', word))[0]
                    for word in vehicle[6:9]]
        report['host_position'] = position
        if enabled != 5 or vehicle[0] != frames or not vehicle[12]:
            raise RuntimeError(f'Authored fighter was not registered: {enabled}, {vehicle}')
        if any(not math.isfinite(value) or (not args.route and abs(value - expected) > 1)
               for value, expected in zip(position, POSITION)):
            raise RuntimeError(f'Unexpected authored fighter position: {position}')
        if args.route:
            route = guest['extra']['rf_scene_vehicle_route_state']
            displacement = math.dist(position, POSITION)
            distances = (math.dist(POSITION, ROUTE_TARGET),
                         math.dist(position, ROUTE_TARGET))
            report['route'] = route
            report['displacement'] = displacement
            report['target_distance'] = distances
            if route[3] < 1 or route[5] != 8644 or displacement < .2 or \
                    distances[1] >= distances[0] - 1:
                raise RuntimeError(f'Authored fighter route did not move: '
                                   f'{route}, displacement {displacement:.3f}, '
                                   f'target distance {distances}')
        else:
            if vehicle[1] != 1 or vehicle[3] != 1:
                raise RuntimeError(f'Authored fighter was not boarded: {vehicle}')
            if weapon[1] < 1 or weapon[6] >= 900:
                raise RuntimeError(f'Authored fighter did not fire minigun: {weapon}')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(folder, 'restore')
        report['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not report['disc_restored']:
            report['result'] = 'FAIL'
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()
