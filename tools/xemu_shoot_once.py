"""Bounded Xbox-only authored Shoot_Once and no-animation fire fixtures.

Reveals L20S1 merc_com UID12544 through authored UnHide UID12475, then
dispatches immediate Shoot_Once UID12546 without walking the level. The
Optional --tankbot dispatches L7S4's secondary missile; --no-animation
stages an L8S4 Parker shot without the 26-second cutscene wait.
--queue stages mixed animated/unanimated requests on Parker in one frame.
The delayed L4S2 event is left for later timing coverage.
Input stays inside the guest replay; no PC game, host input, or images.
"""
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
FRAMES = 110


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--tankbot', action='store_true',
                        help='L7S4 authored secondary missile instead of L20S1 primary')
    mode.add_argument('--no-animation', action='store_true',
                      help='L8S4 authored Parker handgun shot without fire animation')
    mode.add_argument('--queue', action='store_true', help='Three mixed-mode shots on L8S4 Parker, preserving per-request aim')
    args = parser.parse_args()
    parker = args.no_animation or args.queue
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    prefix = 'single-fire-queue-' if args.queue else 'fire-no-animation-' if args.no_animation else 'shoot-once-tankbot-' if args.tankbot else 'shoot-once-'
    folder = ROOT / 'artifacts/xemu' / (prefix +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.update(('player-control.flag', 'campaign-single-fire.bin'))
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox ' +
              ('L8S4 mixed single-fire queue' if args.queue else 'L8S4 Fire_Weapon_No_Anim' if args.no_animation else
               'L7S4 Shoot_Once Tankbot missile' if args.tankbot else 'L20S1 Shoot_Once primary fire')}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') +
            (b'L8S4.rfl' if parker else b'L7S4.rfl' if args.tankbot else b'L20S1.rfl').ljust(64, b'\0'))
        (DISC / 'campaign-setup.bin').write_bytes(
            struct.pack('<I', 6825) if args.queue else struct.pack('<2I', 6825, 8478) if args.no_animation else
            struct.pack('<I', 11132) if args.tankbot else struct.pack('<2I', 12475, 12546))
        if args.queue:(DISC / 'campaign-single-fire.bin').write_bytes(struct.pack('<I',6810))
        (DISC / 'player-control.flag').write_bytes(b'')
        neutral = struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + neutral * FRAMES)
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 300, snapshot=True,
                          extra_symbols={'rf_scene_single_fire_trace':49,
                                         'rf_scene_single_fire_fixture':7,
                                         'rf_scene_script_shoot_once': 6,
                                         'rf_scene_script_fire_no_anim': 6,
                                         'rf_scene_enemy_combat': 8,
                                         'rf_scene_enemy_fire': 6,
                                         'rf_scene_ai_rockets': 5},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        shot = guest['extra']['rf_scene_script_fire_no_anim' if args.no_animation else 'rf_scene_script_shoot_once']
        expected = [0, 8478, 6810, 0] if args.no_animation else [0, 11132, 10696, 0] if args.tankbot else [0, 12546, 12544, 0]
        if not args.queue and (shot[:2] != [1, 1] or shot[2:6] != expected):
            raise RuntimeError(f'Scripted shot not completed: {shot}')
        if guest['extra']['rf_scene_enemy_combat'][2] < 1:
            raise RuntimeError('No finite-ammo NPC fire recorded')
        if args.tankbot and guest['extra']['rf_scene_ai_rockets'][:2] != [1, 1]:
            raise RuntimeError('Tankbot missile launch or impact missing')
        if args.no_animation and guest['extra']['rf_scene_enemy_fire'][1] != 0:
            raise RuntimeError('Fire_Weapon_No_Anim started a firing motion')
        if args.queue:
            extra=guest['extra'];trace=extra['rf_scene_single_fire_trace'];fixture=extra['rf_scene_single_fire_fixture']
            primary=extra['rf_scene_script_shoot_once'];quiet=extra['rf_scene_script_fire_no_anim']
            if primary[:3]!=[1,1,0] or quiet[:3]!=[2,2,0] or primary[5] or quiet[5]:
                raise RuntimeError(f'Mixed requests lost or duplicated: {primary}, {quiet}')
            if trace[0]!=3 or trace[1:10:3]!=[2,0,2] or trace[3:10:3]!=fixture[3:6] or fixture[3]==fixture[4]:
                raise RuntimeError(f'Per-request order/aim changed: {trace[:10]}, {fixture}')
            if fixture[0]!=3 or fixture[1]-fixture[2]!=3 or extra['rf_scene_enemy_fire'][1]!=1:
                raise RuntimeError(f'Ammo or animation mismatch: {fixture}, {extra["rf_scene_enemy_fire"]}')
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
