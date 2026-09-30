"""Exercise staged player sniper and explosion damage against a Fighter on Xbox.

The eye pose is placed by the process-local game fixture beside the hull; the
ordinary player combat tick must select the hit, check world cover, spend ammo
and reduce vehicle vitals. No host input, PC gameplay or capture is used.
"""

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


def as_float(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


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
    folder = ROOT / 'artifacts/xemu' / ('vehicle-player-shot-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
    names |= {'campaign-vehicle-shot.bin', 'player-replay.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Xbox player sniper and blast versus authored Fighter hull'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 4717))
        (DISC / 'campaign-vehicle-shot.bin').write_bytes(struct.pack('<I', 4801))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) + bytes(75 * 44))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, 75, 300, snapshot=True,
                          extra_symbols={'rf_scene_vehicle_shot_probe': 16,
                                         'rf_scene_passive_damage': 8,
                                         'rf_scene_rocket_blast': 8,
                                         'rf_scene_combat_event_count': 1,
                                         'rf_scene_combat': 8,
                                         'rf_scene_player_ammo': 8},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        probe = guest['extra']['rf_scene_vehicle_shot_probe']
        damage = guest['extra']['rf_scene_passive_damage']
        combat = guest['extra']['rf_scene_combat']
        before = (as_float(probe[5]), as_float(probe[6]))
        after = (as_float(probe[8]), as_float(probe[9]))
        report['shot'] = {'probe': probe, 'damage': damage, 'combat': combat,
                          'before': before, 'after': after,
                          'blast': guest['extra']['rf_scene_rocket_blast'],
                          'events': guest['extra']['rf_scene_combat_event_count']}
        if probe[0] != 4801 or not probe[1] or not probe[2] or probe[4] != 1 or \
           probe[11] or combat[0] < 1 or probe[10] != probe[7] - 1 or \
           sum(after) >= sum(before) or damage[2] < 2 or damage[4] != 4801 or \
           probe[15] or as_float(probe[13]) >= as_float(probe[12]) or \
           report['shot']['blast'][2] < 1 or report['shot']['events'][0] < 2:
            raise RuntimeError(f'Player shot and blast did not damage Fighter: {report["shot"]}')
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
