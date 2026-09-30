"""Xbox-only authored rifle pickup, switch and fire; no PC run or images."""
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


def replay(direct_switch=False):
    commands = []
    for frame in range(FRAMES):
        forward = float(10 <= frame < 25)
        fire = int(frame in ((80, 115) if direct_switch else (60, 115)))
        cycle = ((1 if 40 <= frame < 50 or 60 <= frame < 70 else 2 if 50 <= frame < 60 else 0)
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
    args = parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (('weapon-direct-switch-' if args.direct_switch else 'weapon-pickup-') +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'campaign-item.bin', 'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox authored rifle pickup/switch/fire',
              'direct_switch': args.direct_switch}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L4S5.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-item.bin').write_bytes(struct.pack('<I', 3415))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(args.direct_switch))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 180, snapshot=True,
                          extra_symbols={'rf_scene_pickups': 8,
                                         'rf_scene_weapon_selection': 8,
                                         'rf_scene_player_ammo': 8,
                                         'rf_scene_combat': 8})
        result['guest'] = guest
        pickup = guest['extra']['rf_scene_pickups']
        selection = guest['extra']['rf_scene_weapon_selection']
        ammo = guest['extra']['rf_scene_player_ammo']
        combat = guest['extra']['rf_scene_combat']
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
