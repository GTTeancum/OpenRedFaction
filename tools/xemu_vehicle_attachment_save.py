"""Save and reload L20S2's detached fighter on stock-64-MiB Xbox.

Uses only process-local campaign events and the owned ordinary-save HDD.
No desktop input, capture, PC gameplay, or campaign-route replay.
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
SYMBOLS = {'rf_scene_passive_attachment': 14, 'rf_scene_passive_draw': 6,
           'rf_scene_world_snapshot_event_probe': 6}


def build(folder, name):
    with (folder / (name + '-build.log')).open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def pose(words):
    return struct.unpack('<3f', struct.pack('<3I', *words[7:10]))


def main():
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare(ROOT, base)
    folder = ROOT / 'artifacts/xemu' / ('vehicle-attachment-save-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L20S2 group-owned fighter detach ordinary save/reload',
              'phases': {}}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 18354, 18377))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(90 * 44))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, 90, 420, capture_world=True,
                          extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        state = saved['checkpoint_state']
        if state[9] != 1 or state[3] or state[4] < 320:
            raise RuntimeError(f'Xbox save failed: {state}, event probe '
                               f'{saved["extra"]["rf_scene_world_snapshot_event_probe"]}')
        if saved['extra']['rf_scene_passive_attachment'][3] != 1:
            raise RuntimeError('Detached fighter was absent before save')
        payload = (folder / 'save/xbox-world.rfwc').read_bytes()
        directory = 128 + (11 - 1) * 12
        offset, length = struct.unpack_from('<II', payload, directory + 4)
        vehicle = payload[offset:offset + length]
        if vehicle[:4] != b'RFVA':
            raise RuntimeError('Vehicle attachment section is absent')
        version, host_bytes, count = struct.unpack_from('<3I', vehicle, 4)
        if version != 1 or count != 1 or len(vehicle) != 16 + host_bytes + 56:
            raise RuntimeError('Vehicle attachment section is malformed')
        uid, attached = struct.unpack_from('<2I', vehicle, 16 + host_bytes)
        saved_pose = struct.unpack_from('<3f', vehicle, 24 + host_bytes)
        if uid != 4717 or attached:
            raise RuntimeError(f'Detached vehicle state not saved: {uid}, {attached}')
        report['saved_pose'] = saved_pose
        (DISC / 'campaign-setup.bin').unlink()
        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(10 * 44))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, 10, 420, snapshot=True,
                           extra_symbols=SYMBOLS)
        report['phases']['load'] = loaded
        loaded_state = loaded['checkpoint_state']
        attachment = loaded['extra']['rf_scene_passive_attachment']
        if loaded_state[8] != 1 or loaded_state[0] or loaded_state[1] != state[4]:
            raise RuntimeError(f'Xbox load failed: {loaded_state}')
        if attachment[3] != 1 or attachment[4] != 4717:
            raise RuntimeError(f'Detachment not restored: {attachment}')
        if any(abs(a - b) > 0.00001 for a, b in zip(pose(attachment), saved_pose)):
            raise RuntimeError(f'Detached world pose changed: {pose(attachment)} vs {saved_pose}')
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
