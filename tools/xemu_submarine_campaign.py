"""Check one authored L5S3 submarine host on a stock-64-MiB Xbox guest.

The replay is neutral and bounded; it does not traverse the campaign or use
the PC game, screenshots, or host input.
"""
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
EXPECTED = (105.1426773071289, 65.54188537597656, 10.020004272460938)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('submarine-campaign-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox L5S3 authored submarine host'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L5S3.rfl'.ljust(64, b'\0'))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + bytes(FRAMES * 48))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 180, snapshot=True,
                          extra_symbols={'rf_scene_vehicle_enabled': 1,
                                         'rf_scene_vehicle_state': 16},
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
        if enabled != 4 or vehicle[0] != FRAMES or not vehicle[12]:
            raise RuntimeError(f'Authored submarine was not registered: {enabled}, {vehicle}')
        if any(not math.isfinite(value) or abs(value - expected) > 1
               for value, expected in zip(position, EXPECTED)):
            raise RuntimeError(f'Unexpected authored submarine position: {position}')
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
