"""Three process-local Xbox firearm contacts on authored L8S4 Capek.

No campaign traversal, PC runtime, screenshots, or host input. The guest stages
aim and finite ammunition, then uses ordinary player weapon/collision routing.
"""
import datetime
import json
from pathlib import Path
import struct

from xemu_native_world_save import FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu

ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FRAMES = 110


def floating(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (
        'nano-shield-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox Capek firearm shield depletion'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L8S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 10358))
        (DISC / 'campaign-nano-shield.bin').write_bytes(struct.pack('<I', 8359))
        (DISC / 'player-control.flag').write_bytes(b'')
        neutral = struct.pack('<5f7I', *([0] * 12))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + neutral * FRAMES)
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 360, snapshot=True,
                          extra_symbols={'rf_scene_nano_fixture': 25,
                                         'rf_scene_nano_contact': 8,
                                         'rf_scene_combat': 8,
                                         'rf_scene_player_ammo': 8},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}')
        fixture = guest['extra']['rf_scene_nano_fixture']
        contact = guest['extra']['rf_scene_nano_contact']
        rows = [fixture[1+i*8:9+i*8] for i in range(3)]
        report['shots'] = [dict(zip(
            ('health_before', 'armor_before', 'health_after', 'armor_after',
             'ammo_before', 'ammo_after', 'slot', 'frame'),
            [floating(x) for x in row[:4]] + row[4:])) for row in rows]
        if fixture[0] != 3:
            raise RuntimeError(f'Missing staged contacts: {fixture}')
        for i, row in enumerate(rows):
            hb, ab, ha, aa = map(floating, row[:4])
            if row[4] - row[5] != 1 or row[6:] != [[7, 30], [7, 60], [6, 90]][i]:
                raise RuntimeError(f'Finite ammunition/weapon mismatch: {row}')
            if i < 2 and (hb != ha or [ab, aa] != [[150, 50], [50, 0]][i]):
                raise RuntimeError(f'Shield contact leaked health or wrong armor debit: {row}')
            if i == 2 and (ab != 0 or aa != 0 or not ha < hb):
                raise RuntimeError(f'Unshielded contact failed to damage health: {row}')
        if contact[:4] != [2, 1, 0, 8359] or floating(contact[4]) != 100:
            raise RuntimeError(f'Shield contact ownership mismatch: {contact}')
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
