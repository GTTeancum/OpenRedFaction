"""Xbox-only, process-contained idle-NPC ride on the translating DEV platform."""
import argparse
import datetime
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scripted', action='store_true',
                        help='Activate a Goto-style walker during platform translation')
    parser.add_argument('--frames', type=int)
    args = parser.parse_args()
    frames = args.frames if args.frames is not None else 485 if args.scripted else 500
    if frames < 16 or frames > 500:
        parser.error('frames must be 16..500')
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (('npc-script-platform-' if args.scripted else 'npc-platform-') +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    with (folder / 'fixture-build.log').open('wb') as log:
        subprocess.run([sys.executable, '-B', 'tools/build_fragment_platform_fixture.py',
                        '--npc-platform'],
                       cwd=ROOT, check=True, stdout=log)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'fragment-platform-test.flag', 'fragment-platform.vpp', 'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox scripted NPC translating-platform carry'
              if args.scripted else 'Stock-64-MiB Xbox idle NPC translating-platform carry'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        shutil.copyfile(ROOT / 'artifacts/npc-platform/game/levelsm.vpp',
                        DISC / 'fragment-platform.vpp')
        (DISC / 'fragment-platform-test.flag').write_bytes(b'1')
        (DISC / 'dev-npc.flag').write_bytes(b'9' if args.scripted else b'8')
        (DISC / 'dev-room.flag').write_bytes(b'')
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'fragment-platform.vpp'.ljust(64, b'\0') + b'ctf06.rfl'.ljust(64, b'\0'))
        (DISC / 'player-control.flag').write_bytes(b'')
        neutral = struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + neutral * frames)
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, frames, 600, snapshot=True,
                          extra_symbols={'rf_scene_npc_idle_ground': 6,
                                         'rf_scene_npc_mover_support': 8,
                                         'rf_scene_npc_script_mover': 6,
                                         'rf_scene_script_movement': 8,
                                         'rf_scene_npc_platform_probe': 10,
                                         'rf_scene_fragment_platform_audit': 32},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000 or guest['replay_state'][2] != frames:
            raise RuntimeError(f'Xbox guest did not complete: {guest["guest_phase"]}, {guest["replay_state"]}')
        support = guest['extra']['rf_scene_npc_mover_support']
        script = guest['extra']['rf_scene_npc_script_mover']
        idle = guest['extra']['rf_scene_npc_idle_ground']
        platform = guest['extra']['rf_scene_fragment_platform_audit']
        position_words = script[2:5] if args.scripted else support[4:7]
        position = struct.unpack('<3f', struct.pack('<3I', *position_words))
        report['npc_position'] = position
        if frames < (480 if args.scripted else 500):
            report['result'] = 'OBSERVED'
            return
        if idle[4] or not support[1] or support[3] < 50:
            raise RuntimeError(f'NPC did not retain moving support: {idle}, {support}')
        if args.scripted:
            movement = guest['extra']['rf_scene_script_movement']
            if script[0] < 30 or script[1] < 30 or movement[1] < 30 or not 13.2 <= position[0] <= 14.0:
                raise RuntimeError(f'Scripted rider did not walk with moving support: {script}, {movement}, {position}')
        elif support[2] < 50 or not 12.2 <= position[0] <= 12.6:
            raise RuntimeError(f'Idle rider did not follow support: {support}, {position}')
        if platform[1] != 60 or abs(position[2]-2.5) > .25:
            raise RuntimeError(f'NPC did not follow platform displacement: {platform[:3]}, {position}')
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
