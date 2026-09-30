"""Check scripted prop retirement in a stock-64-MiB Xbox guest.

The fixture fires L20S1 Remove_Object12589 at frame zero. It reads the
authoritative registered prop state through XEMU QMP; no host input or capture.
"""

import datetime
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_guest_snapshot import words
from xemu_native_world_save import FLAGS, address, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
TARGET_SLOT = 83  # L20S1 clutter section order: Console Button01 UID12180.


def prop_probe(monitor, mapping):
    owners = words(monitor, address(mapping, 'campaign_clutter_bodies'), 1)[0]
    if not owners:
        raise RuntimeError('Missing campaign clutter owner array')
    owner = words(monitor, owners + TARGET_SLOT * 4, 1)[0]
    if not owner:
        raise RuntimeError('Authored console button has no runtime owner')
    state = words(monitor, owner, 5)
    if state[0] != owner or not state[2]:
        raise RuntimeError(f'Invalid registered clutter state {state}')
    return {'handle': state[2], 'flags': state[4]}


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('remove-clutter-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L20S1 Remove_Object12589 retires console button12180'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 12589))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(60 * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, 60, 360, snapshot=True,
                          probe=prop_probe, probe_frame=20,
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        if not guest['probe']['flags'] & 2:
            raise RuntimeError(f'Console button still active: {guest["probe"]}')
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
