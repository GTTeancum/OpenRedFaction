"""Check an authored campaign submarine host on a stock-64-MiB Xbox guest.

The replay is neutral and bounded; it does not traverse the campaign or use
the PC game, screenshots, or host input.
"""
import argparse
import datetime
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
CASES = {
    'L5S3': ('levels1.vpp', 3963, (30.736595153808594, -16.48705291748047, 8.313613891601562)),
    'L5S4': ('levels1.vpp', 3955, (-27.5601806640625, -24.7811279296875, -14.7227783203125)),
    'L10S3': ('levels2.vpp', 6794, (510.3700256347656, -129.85679626464844, 160.26577758789062)),
}
PURSUE_EVENTS = {
    'L5S3': (4647, None),  # Goto_Player -> UID3963
    'L5S4': (3958, (-12.32318115234375, -22.4365234375, -18.183792114257812)),
}


def replay(board, frames):
    rows = []
    for frame in range(frames):
        rows.append(struct.pack('<5f7I', 0, 0, 0, 0, 0,
                                0, 0, int(board and frame == 12),
                                int(board and frame == 40), 0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--level', choices=CASES, default='L5S3')
    parser.add_argument('--board', action='store_true',
                        help='Stage beside the authored host, board and launch one torpedo')
    parser.add_argument('--pursue', action='store_true',
                        help='Fire the level\'s authored Goto/Goto_Player and check submarine movement')
    parser.add_argument('--save', action='store_true',
                        help='Quick-save/load the active authored pursuit during the Xbox run')
    args = parser.parse_args()
    if args.pursue and (args.board or args.level not in PURSUE_EVENTS):
        parser.error('--pursue requires L5S3 or L5S4 and cannot be combined with --board')
    if args.save and not args.pursue:
        parser.error('--save requires --pursue')
    if args.save and args.level != 'L5S3':
        parser.error('--save currently checks the persistent L5S3 Goto_Player order')
    archive, uid, expected = CASES[args.level]
    event_uid, event_target = PURSUE_EVENTS.get(args.level, (0, None))
    frames = 180 if args.pursue else FRAMES
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('submarine-campaign-' + args.level.lower() + '-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': f'Stock-64-MiB Xbox {args.level} authored submarine host',
              'board_fixture': args.board, 'pursue_fixture': args.pursue,
              'save_fixture': args.save, 'uid': uid}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            archive.encode().ljust(64, b'\0') + f'{args.level}.rfl'.encode().ljust(64, b'\0'))
        if args.board:
            (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', uid))
        if args.pursue:
            (DISC / 'campaign-goto.bin').write_bytes(struct.pack('<II', event_uid, 1))
        if args.save:
            (DISC / 'campaign-quick-actions.bin').write_bytes(struct.pack('<II', 60, 100))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(args.board, frames))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, frames, 180, snapshot=True,
                          extra_symbols={'rf_scene_vehicle_enabled': 1,
                                         'rf_scene_vehicle_state': 16,
                                         'rf_scene_vehicle_route_state': 8,
                                         'rf_scene_submarine_weapon': 8,
                                         'rf_scene_submarine_boarding': 8,
                                         'rf_scene_player_swim': 12},
                          allow_guest_error=True)
        result['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        enabled = guest['extra']['rf_scene_vehicle_enabled'][0]
        vehicle = guest['extra']['rf_scene_vehicle_state']
        position = [struct.unpack('<f', struct.pack('<I', word))[0]
                    for word in vehicle[6:9]]
        result['host_position'] = position
        result['boarding'] = guest['extra']['rf_scene_submarine_boarding']
        result['swim'] = guest['extra']['rf_scene_player_swim']
        if enabled != 4 or (not args.save and vehicle[0] != frames) or \
                (args.save and not 0 < vehicle[0] < frames) or not vehicle[12]:
            raise RuntimeError(f'Authored submarine was not registered: {enabled}, {vehicle}')
        if any(not math.isfinite(value) or (not args.pursue and abs(value - expected) > 1)
               for value, expected in zip(position, expected)):
            raise RuntimeError(f'Unexpected authored submarine position: {position}')
        if args.pursue:
            route = guest['extra']['rf_scene_vehicle_route_state']
            displacement = math.dist(position, expected)
            result['route'] = route
            result['displacement'] = displacement
            if route[3] < 1 or route[5] != event_uid or displacement < .05:
                raise RuntimeError(f'Authored Goto order did not move submarine: '
                                   f'{route}, displacement {displacement:.3f}')
            if event_target is not None:
                start_distance = math.dist(expected, event_target)
                end_distance = math.dist(position, event_target)
                result['target_distance'] = [start_distance, end_distance]
                if end_distance >= start_distance - .05:
                    raise RuntimeError(f'Authored Goto did not approach its target: '
                                       f'{start_distance:.3f} -> {end_distance:.3f}')
            if args.save:
                state = guest['checkpoint_state']
                if state[8] != 1 or state[0] != 0 or route[0] != 1:
                    raise RuntimeError(f'Authored submarine pursuit did not resume after '
                                       f'quick-load: checkpoint {state}, route {route}')
        if args.board:
            weapon = guest['extra']['rf_scene_submarine_weapon']
            if vehicle[1] != 1 or vehicle[3] != 1:
                raise RuntimeError(f'Authored submarine was not boarded: {vehicle}')
            if weapon[1] != 1 or weapon[7] != 19:
                raise RuntimeError(f'Authored submarine did not launch a torpedo: {weapon}')
        result['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(folder, 'restore')
        result['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not result['disc_restored']:
            result['result'] = 'FAIL'
        (folder / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        print(folder, result['result'], flush=True)
        if not result['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()
