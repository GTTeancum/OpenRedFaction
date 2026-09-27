"""Xbox-only bounded submarine ordinary save/reload in the underwater DEV room.

Uses an owned test HDD and process-contained replay; no PC game or host input.
"""
import datetime
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
SAVE_FRAMES = 140
LOAD_FRAMES = 80


def replay(frames, move):
    rows = []
    for frame in range(frames):
        forward = float(move and 40 <= frame < 95)
        rise = int(move and 110 <= frame < 125)
        use = int(move and frame == 12)
        rows.append(struct.pack('<5f7I', 0, 0, forward, 0, 0,
                                0, rise, use, 0, 0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare(ROOT, base)
    folder = ROOT / 'artifacts/xemu' / ('submarine-save-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox submarine ordinary save/reload',
              'hdd': str(hdd)}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L5S3.rfl'.ljust(64, b'\0'))
        (DISC / 'dev-room.flag').write_bytes(b'')
        (DISC / 'vehicle-test.flag').write_bytes(b'4')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(SAVE_FRAMES, True))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, SAVE_FRAMES, 180,
                          extra_symbols={'rf_scene_vehicle_state': 16,
                                         'rf_scene_submarine_weapon': 8,
                                         'rf_scene_player_checkpoint_state': 8,
                                         'rf_scene_world_snapshot_event_probe': 6},
                          allow_guest_error=True)
        result['save'] = saved
        if saved['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox save guest error {saved["guest_phase"]:08x}: '
                               f'{saved["checkpoint_state"]}, '
                               f'player {saved["extra"]["rf_scene_player_checkpoint_state"]}, '
                               f'events {saved["extra"]["rf_scene_world_snapshot_event_probe"]}')
        state = saved['checkpoint_state']
        if state[9] != 1 or state[3] != 0 or not 320 <= state[4] <= 110524:
            raise RuntimeError(f'Xbox submarine ordinary save failed: {state}')
        if saved['extra']['rf_scene_vehicle_state'][3] != 1:
            raise RuntimeError('Xbox submarine was not boarded at save endpoint')
        if saved['extra']['rf_scene_submarine_weapon'][7] != 20:
            raise RuntimeError('Unexpected torpedo reserve at save endpoint')

        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(replay(LOAD_FRAMES, False))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, LOAD_FRAMES, 180,
                           extra_symbols={'rf_scene_vehicle_state': 16,
                                          'rf_scene_submarine_weapon': 8})
        result['load'] = loaded
        state = loaded['checkpoint_state']
        if state[8] != 1 or state[0] != 0 or state[1] != saved['checkpoint_state'][4]:
            raise RuntimeError(f'Xbox submarine ordinary reload failed: {state}')
        if loaded['extra']['rf_scene_vehicle_state'][3] != 1:
            raise RuntimeError('Xbox submarine occupancy was not restored')
        if loaded['extra']['rf_scene_submarine_weapon'][7] != 20:
            raise RuntimeError('Xbox submarine torpedo reserve was not restored')
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
