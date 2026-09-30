"""Xbox-only authored weapon and suit pickup checks; no PC run or images."""
import datetime
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FRAMES = 140


def replay(direct_switch=False, nonweapon=False, silenced=False):
    commands = []
    for frame in range(FRAMES):
        forward = float(10 <= frame < (80 if silenced else 40 if nonweapon else 25))
        fire = 0 if nonweapon or silenced else int(frame in ((80, 115) if direct_switch else (60, 115)))
        cycle = 0 if nonweapon or silenced else ((1 if 40 <= frame < 50 or 60 <= frame < 70 else 2 if 50 <= frame < 60 else 0)
                 if direct_switch else (1 if frame in (40, 105) else 2 if frame == 90 else 0))
        commands.append(struct.pack('<5f6I', 0, 0, forward, 0, 0,
                                    0, 0, 0, fire, 0, cycle))
    return b'RFI5' + struct.pack('<I', 44) + b''.join(commands)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--direct-switch', action='store_true',
                        help='hold next, then previous, then next with no neutral input frames')
    parser.add_argument('--nonweapon', action='store_true',
                        help='stage the authored L6S3 Miner Envirosuit scripted grant')
    parser.add_argument('--silenced', action='store_true',
                        help='stage the authored train02 Silenced 12mm Handgun pickup')
    args = parser.parse_args()
    if sum((args.direct_switch, args.nonweapon, args.silenced)) > 1:
        parser.error('Choose only one focused fixture')
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (('silenced-pickup-' if args.silenced else
                                       'nonweapon-pickup-' if args.nonweapon else
                                       'weapon-direct-switch-' if args.direct_switch else 'weapon-pickup-') +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'campaign-item.bin', 'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox authored item grant',
              'direct_switch': args.direct_switch, 'nonweapon': args.nonweapon,
              'silenced': args.silenced}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') +
            (b'L6S3.rfl' if args.nonweapon else b'train02.rfl' if args.silenced else b'L4S5.rfl').ljust(64, b'\0'))
        (DISC / 'campaign-item.bin').write_bytes(struct.pack('<I',
            6935 if args.nonweapon else 6596 if args.silenced else 3415))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(args.direct_switch, args.nonweapon, args.silenced))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 180, snapshot=True,
                          extra_symbols={'rf_scene_pickups': 8,
                                         'rf_scene_player_spawn_diagnostic': 19,
                                         'rf_scene_actor_follow_frames': 896,
                                         'rf_scene_nonweapon_items': 4,
                                         'rf_scene_script_grants': 8,
                                         'rf_scene_pickup_notice': 16,
                                         'rf_scene_weapon_selection': 8,
                                         'rf_scene_player_ammo': 8,
                                         'rf_scene_combat': 8})
        result['guest'] = guest
        pickup = guest['extra']['rf_scene_pickups']
        selection = guest['extra']['rf_scene_weapon_selection']
        ammo = guest['extra']['rf_scene_player_ammo']
        combat = guest['extra']['rf_scene_combat']
        if args.nonweapon:
            nonweapon = guest['extra']['rf_scene_nonweapon_items']
            script = guest['extra']['rf_scene_script_grants']
            notice = struct.pack('<16I', *guest['extra']['rf_scene_pickup_notice']).split(b'\0', 1)[0]
            if nonweapon[2] != 1 or nonweapon[3] != 28 or script[0] < 1 or script[3] != 0xfffffffe:
                raise RuntimeError(f'Miner Envirosuit scripted grant failed: {nonweapon}, {script}')
            if notice != b'Miner Envirosuit picked up':
                raise RuntimeError(f'Authored pickup notice not published: {notice!r}')
            result['placed_pickup_collected'] = pickup[3] == 1 and pickup[5] == 6935
            result['result'] = 'PASS'
            return
        if args.silenced:
            if pickup[3] != 1 or pickup[5] != 6596:
                raise RuntimeError(f'Silenced handgun was not collected: {pickup}')
            result['result'] = 'PASS'
            return
        if pickup[3:6] != [1, 42, 3415]:
            raise RuntimeError(f'Rifle was not collected: {pickup}')
        if selection[0] != 1 or selection[1] != 3 or selection[3] != 1:
            raise RuntimeError(f'Forward/back/forward weapon switches failed: {selection}')
        if ammo[0] != selection[2] or combat[0] < 1 or ammo[2] >= 42:
            raise RuntimeError(f'Equipped rifle did not fire: ammo {ammo}, combat {combat}')
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
