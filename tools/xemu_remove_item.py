"""Check one authored Remove_Object item link on a stock-64-MiB Xbox guest.

Uses an isolated XEMU HDD and process-local setup; no host input or capture.
"""

import argparse
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
CASES = {
    'demo': ('levels1.vpp', 'L2S2a.rfl', 8479, 5453),
    'miner-suit': ('levels2.vpp', 'L8S4.rfl', 10333, 8614),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, default='demo')
    args = parser.parse_args()
    archive, level, event_uid, item_uid = CASES[args.case]
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('remove-item-' + args.case + '-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': f'{level} Remove_Object{event_uid} retires placed item{item_uid}'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            archive.encode('ascii').ljust(64, b'\0') + level.encode('ascii').ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', event_uid))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(60 * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, 60, 360, snapshot=True,
                          extra_symbols={'rf_scene_campaign_pickups': 2242,
                                         'rf_scene_pickups': 8},
                          allow_guest_error=True)
        ledger = guest['extra'].pop('rf_scene_campaign_pickups')
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest stopped before item dispatch: '
                               f'phase {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        count = ledger[1]
        if count > 64:
            raise RuntimeError(f'Unexpected pickup ledger count {count}')
        records = [ledger[2050 + i*3:2053 + i*3] for i in range(count)]
        matches = [record for record in records if record[1] == item_uid]
        if len(matches) != 1 or matches[0][2] != 1:
            raise RuntimeError(f'Authored item not retired: {matches}')
        if guest['extra']['rf_scene_pickups'][3] != 0:
            raise RuntimeError('Remove_Object unexpectedly granted a pickup')
        report.update(pickup_ledger_count=count,
                      retired_item_record=matches[0], result='PASS')
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
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)


if __name__ == '__main__':
    main()
