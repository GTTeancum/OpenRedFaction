"""Bounded stock-64-MiB Xbox check of L20S3's authored Defuse_Nuke entry.

The replay supplies directions inside the guest process. It does not control
the host desktop, play a campaign route, capture images, or run PC gameplay.
"""
import datetime
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
UID = 18307
FRAMES = 140


def solution():
    state = UID ^ 0x5a17c0de
    result = []
    for _ in range(11):
        state = (state * 214013 + 2531011) & 0xffffffff
        result.append(((state >> 16) & 0x7fff) % 4 + 1)
    return result


def replay():
    rows = []
    pattern = solution()
    for frame in range(FRAMES):
        symbol = pattern[(frame - 20) // 3] if frame >= 20 and \
            (frame - 20) % 3 == 0 and (frame - 20) // 3 < len(pattern) else 0
        vertical = 1.0 if symbol == 1 else -1.0 if symbol == 3 else 0.0
        cycle = 2 if symbol == 2 else 1 if symbol == 4 else 0
        rows.append(struct.pack('<5f7I', 0, vertical, 0, 0, 0,
                                0, 0, 0, 0, 0, cycle, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, name):
    with (folder / f'{name}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('defuse-l20s3-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox L20S3 Defuse_Nuke',
              'event_uid': UID, 'trigger_uid': 18306, 'solution': solution()}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S3.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-trigger-start.bin').write_bytes(struct.pack('<I', 18306))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay())
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 180, snapshot=True,
                          extra_symbols={'rf_scene_defuse': 13,
                                         'rf_scene_endgame': 6,
                                         'rf_scene_event_ticks': 12},
                          allow_guest_error=True)
        report['guest'] = guest
        puzzle = guest['extra']['rf_scene_defuse']
        ending = guest['extra']['rf_scene_endgame']
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        if puzzle[0] != 1 or puzzle[2] != 11 or puzzle[6] != 1 or puzzle[7] != 0:
            raise RuntimeError(f'Defuse sequence did not complete: {puzzle}')
        if puzzle[12] < 40 or guest['extra']['rf_scene_event_ticks'][0] > FRAMES - puzzle[12]:
            raise RuntimeError(f'Modal gameplay kept advancing: {puzzle}, '
                               f'{guest["extra"]["rf_scene_event_ticks"]}')
        if ending[2] != 1 or ending[4] != 2:
            raise RuntimeError(f'Credits outcome missing: {ending}')
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
