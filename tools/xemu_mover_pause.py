"""Check an authored tram pause/stop through stock-64-MiB Xbox game state.

L14S2 Mover_Pause9879 pauses Tram01 at setup. Invert10201 sends it an OFF
action at frame 60. The guest remains isolated; no host input or capture.
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


def pause_probe(monitor, mapping):
    on, off, stops, handle, flags, next_key = words(
        monitor, address(mapping, 'rf_scene_mover_pause'), 6)
    return {'on': on, 'off': off, 'stops': stops, 'handle': handle,
            'flags': flags, 'next_key': next_key}


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('mover-pause-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L14S2 Tram01 ON pause and Invert OFF stop'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels3.vpp'.ljust(64, b'\0') + b'L14S2.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 9879, 10201))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(90 * 44))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, 90, 420, snapshot=True,
                          probe=pause_probe, probe_frame=20,
                          extra_symbols={'rf_scene_mover_pause': 6},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        first = guest['probe']
        final = guest['extra']['rf_scene_mover_pause']
        if first['on'] != 1 or first['off'] or first['stops'] or not first['flags'] & 0x80:
            raise RuntimeError(f'Tram did not hold while paused: {first}')
        if final[:3] != [1, 1, 1] or final[4] & 0x80 or final[5] != 0xffffffff:
            raise RuntimeError(f'Tram did not stop after OFF: {final}')
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
