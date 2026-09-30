"""Xbox-only NPC landing check on a held, rocket-extracted CTF06 fragment."""
import datetime
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from xemu_native_world_save import DISC, FLAGS, ROOT, run_guest
from xemu_session_guard import require_no_project_xemu


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    moving = '--moving' in sys.argv
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    source = ROOT / 'artifacts/beam-center-fragments/1.bin'
    if not hdd.is_file() or not source.is_file():
        raise RuntimeError('Missing isolated Xbox HDD or existing extraction input')
    input_data = source.read_bytes()
    if input_data[:8] != b'RFI6' + struct.pack('<I', 48) or len(input_data) != 8 + 600 * 48:
        raise RuntimeError('Unexpected extraction replay format')
    input_data = input_data[:8 + 300 * 48]
    frames = 430
    folder = ROOT / 'artifacts/xemu' / (('npc-rubble-carry-' if moving else 'npc-rubble-support-') +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'authored-source.bin', 'authored-count.bin', 'player-control.flag',
              'fragment-platform-test.flag', 'fragment-platform.vpp',
              'geomod-checkpoint.bin', 'geomod-checkpoint-out.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox NPC support on real extracted GeoMod fragment',
              'moving': moving}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'dev-room.flag').write_bytes(b'')
        (DISC / 'dev-npc.flag').write_bytes(b'B' if moving else b'A')
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levelsm.vpp'.ljust(64, b'\0') + b'ctf06.rfl'.ljust(64, b'\0'))
        (DISC / 'authored-source.bin').write_bytes(struct.pack('<I', 95))
        (DISC / 'authored-count.bin').write_bytes(struct.pack('<I', 3))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(input_data + bytes((frames - 300) * 48))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, frames, 600, snapshot=True,
                          extra_symbols={'rf_scene_dev_npc_rubble_support': 20,
                                         'rf_scene_npc_idle_ground': 6,
                                         'rf_scene_npc_mover_support': 8,
                                         'rf_scene_rockets': 8,
                                         'rf_scene_geomod': 8,
                                         'rf_scene_detached_pieces': 6,
                                         'rf_scene_detached_rocket': 7},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000 or guest['replay_state'][2] != frames:
            raise RuntimeError('Xbox guest did not complete')
        support = guest['extra']['rf_scene_dev_npc_rubble_support']
        idle = guest['extra']['rf_scene_npc_idle_ground']
        report['support'] = support
        report['idle_ground'] = idle
        report['npc_y'] = struct.unpack('<f', struct.pack('<I', support[10]))[0]
        report['contact_normal_y'] = struct.unpack('<f', struct.pack('<I', support[6]))[0]
        report['contact_time'] = struct.unpack('<f', struct.pack('<I', support[7]))[0]
        report['contact_y'] = struct.unpack('<f', struct.pack('<I', support[14]))[0]
        report['fragment_top_y'] = struct.unpack('<f', struct.pack('<I', support[15]))[0]
        report['npc_start_x'] = struct.unpack('<f', struct.pack('<I', support[16]))[0]
        report['npc_end_x'] = struct.unpack('<f', struct.pack('<I', support[17]))[0]
        report['fragment_end_x'] = struct.unpack('<f', struct.pack('<I', support[18]))[0]
        if not support[0] or not .5 < struct.unpack('<f', struct.pack('<I', support[3]))[0]:
            raise RuntimeError(f'No admitted extracted fragment: {support}')
        if (not support[4] or support[5] != 0xffffffff or report['contact_normal_y'] < .5 or
                abs(report['contact_y'] - report['fragment_top_y']) > .02):
            raise RuntimeError(f'NPC ground probe missed rubble: {support}')
        if support[8] < 1 or support[9] == 3 or idle[4] or support[11]:
            raise RuntimeError(f'NPC did not land and remain supported: {support}, {idle}')
        if moving and (report['npc_end_x'] - report['npc_start_x'] < .7 or support[19] < 40):
            raise RuntimeError(f'NPC did not follow the moving fragment: {support}')
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
