"""Xbox-only submerged guard fixture for torpedo acquisition and damage.

The guard is opt-in and process-contained. No PC game, images, or host input.
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
FRAMES = 200


def replay():
    rows = []
    for frame in range(FRAMES):
        rows.append(struct.pack('<5f7I', 0, 0, 0, 0, 0,
                                0, 0, int(frame == 12), int(frame == 40),
                                0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


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
    folder = ROOT / 'artifacts/xemu' / ('submarine-target-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox submerged torpedo target'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L5S3.rfl'.ljust(64, b'\0'))
        (DISC / 'dev-room.flag').write_bytes(b'')
        (DISC / 'vehicle-test.flag').write_bytes(b'4')
        (DISC / 'dev-npc.flag').write_bytes(b'7')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay())
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 180, snapshot=True,
                          extra_symbols={'rf_scene_vehicle_state': 16,
                                         'rf_scene_submarine_weapon': 8,
                                         'rf_scene_submarine_impact_state': 10,
                                         'rf_scene_submarine_homing_state': 8},
                          allow_guest_error=True)
        result['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        vehicle = guest['extra']['rf_scene_vehicle_state']
        torpedo = guest['extra']['rf_scene_submarine_weapon']
        impact = guest['extra']['rf_scene_submarine_impact_state']
        result['impact'] = impact
        homing = guest['extra']['rf_scene_submarine_homing_state']
        result['homing'] = homing
        if vehicle[3] != 1 or torpedo[1] != 1 or torpedo[7] != 19:
            raise RuntimeError(f'Torpedo was not fired from occupied submarine: {vehicle}, {torpedo}')
        if not homing[0] or not homing[1] or not homing[3] or not homing[4]:
            raise RuntimeError(f'Wet hostile acquisition/steering failed: {homing}')
        if not torpedo[2] or not torpedo[6]:
            raise RuntimeError(f'Torpedo did not hit and detonate: {torpedo}')
        if impact[1] != 1:
            raise RuntimeError(f'Torpedo did not make direct NPC contact: {impact}')
        health = struct.unpack('<f', struct.pack('<I', homing[5]))[0]
        if not math.isfinite(health) or health >= 50:
            raise RuntimeError(f'Submerged guard took no damage: {health}')
        result['target_health'] = health
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
