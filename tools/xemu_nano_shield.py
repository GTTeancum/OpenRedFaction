"""Three process-local Xbox weapon contacts on authored L8S4 Capek.

No campaign traversal, PC runtime, screenshots, or host input. The guest stages
aim and finite ammunition; grenade mode stages releases and reserve debits,
then uses actual flight/contact handling. Other modes use player weapon routing.
"""
import argparse
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
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--rockets', action='store_true',
                        help='Two real rocket flights, then a sniper health hit')
    mode.add_argument('--grenades', action='store_true',
                      help='Staged normal/alternate shield contacts, then an ordinary actor hit')
    mode.add_argument('--remotes', action='store_true',
                      help='Shield absorption, shield-off attachment, then ordinary detonation')
    args = parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (
        ('nano-shield-remotes-' if args.remotes else 'nano-shield-grenades-' if args.grenades else 'nano-shield-rockets-' if args.rockets else 'nano-shield-') + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox Capek shield depletion', 'rockets': args.rockets, 'grenades': args.grenades, 'remotes': args.remotes}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L8S4.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 10358))
        (DISC / 'campaign-nano-shield.bin').write_bytes(struct.pack('<2I', 8359, 3 if args.remotes else 2 if args.grenades else int(args.rockets)))
        (DISC / 'player-control.flag').write_bytes(b'')
        neutral = struct.pack('<5f7I', *([0] * 12))
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + neutral * FRAMES)
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 360, snapshot=True,
                          extra_symbols={'rf_scene_nano_fixture': 25,
                                         'rf_scene_nano_contact': 8,
                                         'rf_scene_nano_aim': 12,
                                         'rf_scene_grenade_contacts': 8,
                                         'rf_scene_grenades': 8,
                                         'rf_scene_remote': 8,
                                         'rf_scene_remote_shield': 4,
                                         'rf_scene_combat': 8,
                                         'rf_scene_player_ammo': 8,
                                         'rf_scene_rockets': 8,
                                         'rf_scene_rocket_blast': 8},
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
        debit = floating(contact[4]) if args.rockets or args.grenades or args.remotes else 100
        slot = 5 if args.grenades else 4 if args.rockets else 7
        for i, row in enumerate(rows):
            hb, ab, ha, aa = map(floating, row[:4])
            expected_slots = [[8, 30], [8, 60], [9, 90]] if args.remotes else [[slot, 30], [slot, 60], [5 if args.grenades else 6, 90]]
            expected_ammo = 0 if args.remotes and i == 2 else 1
            if row[4] - row[5] != expected_ammo or row[6:] != expected_slots[i]:
                raise RuntimeError(f'Finite ammunition/weapon mismatch: {row}')
            expected_armor = [[100, 100], [100, 100]] if args.remotes else [[100, 100], [100, 0]] if args.grenades else [[debit * 1.5, debit * .5], [debit * .5, 0]]
            if i < 2 and (hb != ha or [ab, aa] != expected_armor[i]):
                raise RuntimeError(f'Shield contact leaked health or wrong armor debit: {row}')
            if i == 2 and (not ha < hb or (not args.remotes and (ab != 0 or aa != 0))):
                raise RuntimeError(f'Unshielded contact failed to damage health: {row}')
        if args.remotes:
            remote = guest['extra']['rf_scene_remote']
            absorbed = guest['extra']['rf_scene_remote_shield']
            if contact[:4] != [1, 0, 0, 8359] or debit != 0 or absorbed[:2] != [1, 8359]:
                raise RuntimeError(f'Remote shield armor/contact mismatch: {contact}, {absorbed}')
            if remote[1:5] != [2, 1, 1, 0] or guest['extra']['rf_scene_rocket_blast'][0] != 1:
                raise RuntimeError(f'Remote absorption/attachment/detonation mismatch: {remote}')
        elif contact[:4] != [2, 1, 0, 8359] or debit <= 0 or floating(contact[4]) != debit:
            raise RuntimeError(f'Shield contact ownership mismatch: {contact}')
        if args.grenades:
            grenades = guest['extra']['rf_scene_grenades']
            hits = guest['extra']['rf_scene_grenade_contacts']
            if grenades[1:5] != [3, 0, 1, 0] or hits[:4] != [3, 2, 1, 8359] or floating(hits[4]) <= 0:
                raise RuntimeError(f'Grenade retirement, direct damage or burst mismatch: {grenades}, {hits}')
            if guest['extra']['rf_scene_rocket_blast'][0] != 1 or guest['extra']['rf_scene_rockets'][4]:
                raise RuntimeError('Grenade shield contact leaked a blast or object contact cut terrain')
        if args.rockets:
            flights = guest['extra']['rf_scene_rockets']
            blasts = guest['extra']['rf_scene_rocket_blast']
            if flights[:4] != [2, 2, 0, 0] or flights[4] or blasts[0]:
                raise RuntimeError(f'Shield contact did not consume flights before blast/terrain: {flights}, {blasts}')
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
