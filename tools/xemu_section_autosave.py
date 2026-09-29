"""Xbox-only authored section handoff, automatic save, and death recovery.

The exit and fatal hit are process-local fixtures. No campaign route, PC game,
host input, screenshot, or user HDD is involved.
"""
import argparse
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
FRAMES = 300


def replay(frames):
    rows = [struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0,
                        int(frame == 220), 0, 0, 0, 0)
            for frame in range(frames)]
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe-only', action='store_true',
                        help='Stop after the new-section save attempt')
    args = parser.parse_args()
    frames = 190 if args.probe_only else FRAMES
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('section-autosave-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'player-control.flag', 'campaign-player-kill.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox section autosave and death recovery'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-exit.bin').write_bytes(struct.pack('<I', 9019))
        if not args.probe_only:
            (DISC / 'campaign-player-kill.bin').write_bytes(struct.pack('<I', 90))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(frames))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, frames, 360, snapshot=True,
                          extra_symbols={'rf_scene_section_autosave': 4,
                                         'rf_scene_player_kill_test': 4},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        autosave = guest['extra']['rf_scene_section_autosave']
        if autosave[:3] != [1, 1, 0] or not 30 <= autosave[3] <= 180:
            raise RuntimeError(f'No valid section autosave: {guest["extra"]}')
        if args.probe_only:
            if guest['level_transitions'][:2] != [1, 9019]:
                raise RuntimeError(f'Wrong section handoff: {guest["level_transitions"]}')
            report['result'] = 'PASS'
            return
        if guest['level_transitions'][:2] != [2, 0xfffffffd] or \
                guest['player_life'][2] or guest['replay_state'][2] != frames:
            raise RuntimeError(f'Wrong recovery: transitions {guest["level_transitions"]}, '
                               f'life {guest["player_life"]}, replay {guest["replay_state"]}')
        if guest['checkpoint_state'][0] != 0 or not guest['checkpoint_state'][1]:
            raise RuntimeError(f'Autosave was not restored: {guest["checkpoint_state"]}')
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
