"""Bounded Xbox-only delayed-event section return with live authored references.

The process-local fixture forces two authored exits. It does not walk a route,
send host input, capture images, use a PC executable, or alter the user's HDD.
"""

import datetime
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FRAMES = 360


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--retired-startup', action='store_true',
                        help='Check that an authored Remove_Object target stays absent before startup')
    args = parser.parse_args()
    frames = 240 if args.retired_startup else FRAMES
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('pending-section-event-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': ('Stock-64-MiB Xbox retired startup event return'
              if args.retired_startup else 'Stock-64-MiB Xbox delayed Message section return')}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') +
            (b'L1S1.rfl' if args.retired_startup else b'L1S2.rfl').ljust(64, b'\0'))
        if args.retired_startup:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 8630))
            (DISC / 'campaign-setup-immediate.flag').write_bytes(b'')
            (DISC / 'campaign-exit.bin').write_bytes(struct.pack('<I', 9019))
            (DISC / 'campaign-return.bin').write_bytes(struct.pack('<2I', 9346, 0))
        else:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<3I', 9725, 9728, 9732))
            (DISC / 'campaign-disable-auto.bin').write_bytes(struct.pack('<I', 10078))
            (DISC / 'campaign-exit.bin').write_bytes(struct.pack('<I', 9346))
            (DISC / 'campaign-return.bin').write_bytes(struct.pack('<2I', 9019, 0))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(frames * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, frames, 480, snapshot=True,
                          extra_symbols={'rf_xbox_event_handoff_probe': 2,
                                         'rf_scene_startup_events': 9,
                                         'rf_scene_startup_inventory': 4,
                                         'campaign_subtitle_uid': 1,
                                         'campaign_subtitle_deadline': 1},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}; '
                               f'load stage {guest["campaign_load_stage"]}')
        if guest['level_transitions'][:2] != [2, 9346 if args.retired_startup else 9019]:
            raise RuntimeError(f'Wrong section return: {guest["level_transitions"]}')
        if args.retired_startup:
            inventory = guest['extra']['rf_scene_startup_inventory']
            if inventory[:2] != [1, 0] or guest['extra']['rf_scene_startup_events'][4] < 1:
                raise RuntimeError(f'Retired startup event replayed: {guest["extra"]}')
        else:
            before, after = guest['extra']['rf_xbox_event_handoff_probe']
            if (before, after) != (15, 7):
                raise RuntimeError(f'Pending event refs/dispatch failed: {before}, {after}')
            if guest['extra']['campaign_subtitle_uid'][0] != 9725 or \
               guest['extra']['campaign_subtitle_deadline'][0] == 0xffffffff:
                raise RuntimeError(f'Delayed Message did not present after return: {guest["extra"]}')
            if guest['extra']['rf_scene_startup_events'][0] != 5:
                raise RuntimeError(f'Disabled auto trigger repeated on return: {guest["extra"]}')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        with (folder / 'restore-build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
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
