"""Exercise L7S4's six authored door reversals in a stock-64-MiB Xbox guest.

An immediate Delay event starts the doors at setup; the authored
Reverse_Mover fires one second later while their three-second motion is active.
The guest is inspected through XEMU QMP without host input or capture.
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


def reverse_probe(monitor, mapping):
    state = words(monitor, address(mapping, 'rf_scene_reverse_mover'), 4)
    return {'requests': state[0], 'reversals': state[1],
            'last_controller': state[2], 'last_direction': state[3]}


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('reverse-mover-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L7S4 six moving doors reverse at frame 60'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L7S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 11163, 11274))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(100 * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, 100, 420, snapshot=True,
                          probe=reverse_probe,
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        state = guest['probe']
        if state['requests'] != 6 or state['reversals'] != 6 or state['last_direction'] != 1:
            raise RuntimeError(f'Unexpected door reversal state: {state}')
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
