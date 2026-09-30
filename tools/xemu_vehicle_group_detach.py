"""Check a group-owned vehicle pose and Detach on stock-64-MiB Xbox.

L20S2 When_Dead18354 is process-locally fired to activate Hanger Lift001;
Detach18377 is fired at frame 60 for its masako_fighter4717 child. No
desktop input or capture is used.
"""

import datetime
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

from xemu_guest_snapshot import words
from xemu_native_world_save import FLAGS, address, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
ORIGINAL_X = 416.25457763671875


def attachment_probe(monitor, mapping):
    return words(monitor, address(mapping, 'rf_scene_passive_attachment'), 14)


def position(row):
    return struct.unpack('<3f', struct.pack('<3I', *row[7:10]))


def main():
    inventory = sys.argv[1:] == ['--inventory']
    submarine = sys.argv[1:] == ['--submarine']
    short = inventory or submarine
    if sys.argv[1:] and not short:
        raise SystemExit('usage: xemu_vehicle_group_detach.py [--inventory|--submarine]')
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (('vehicle-group-submarine-' if submarine else
             'vehicle-group-inventory-' if inventory
             else 'vehicle-group-detach-') +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L5S3 group-owned submarine3977' if submarine
              else 'L20S1 three group-owned Fighters' if inventory
              else 'L20S2 vehicle4717 lift pose and Detach18377'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            (b'levels1.vpp' if submarine else b'levels2.vpp').ljust(64, b'\0') +
            (b'L5S3.rfl' if submarine else b'L20S1.rfl' if inventory else b'L20S2.rfl').ljust(64, b'\0'))
        if not short:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 18354, 18377))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes((10 if short else 90) * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, 10 if short else 90, 420, snapshot=True,
                          probe=None if short else attachment_probe, probe_frame=35,
                          extra_symbols={'rf_scene_passive_attachment': 14},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        final = guest['extra']['rf_scene_passive_attachment']
        if inventory:
            if final[0:2] != [3, 6] or final[13]:
                raise RuntimeError(f'Three Fighter owners/six bindings absent: {final}')
        elif submarine:
            if final[0:2] != [1, 1] or final[4] != 3977 or final[13]:
                raise RuntimeError(f'Moving submarine owner/binding absent: {final}')
        else:
            first = guest['probe']
            if first[0] < 1 or first[1] < 1 or first[2] < 1 or first[3] or first[4] != 4717:
                raise RuntimeError(f'Vehicle did not follow lift before Detach: {first}')
            if final[3] != 1 or final[4] != 4717 or final[6] != 0xffffffff:
                raise RuntimeError(f'Vehicle was not detached: {final}')
            if final[7:10] != final[10:13] or final[13]:
                raise RuntimeError(f'Vehicle did not retain its detach pose: {final}')
            for label, row in (('before', first), ('detached', final)):
                xyz = position(row)
                if not all(math.isfinite(v) for v in xyz) or xyz[0] < ORIGINAL_X + .2:
                    raise RuntimeError(f'{label} pose did not follow lift: {xyz}')
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
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)


if __name__ == '__main__':
    main()
