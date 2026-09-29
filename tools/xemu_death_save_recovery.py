"""Bounded stock-64-MiB Xbox save/death recovery check; no route or host input.

Save a settled L1S1 player, deliver one process-local ordinary damage hit,
then press Use inside the guest after the death gate.
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
FRAMES = 235


def replay():
    rows = [struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0,
                        int(frame == 175), 0, 0, 0, 0)
            for frame in range(FRAMES)]
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-save', action='store_true',
                        help='Check the fresh-section fallback with no new save')
    args = parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('death-save-recovery-' +
              ('fallback-' if args.no_save else '') +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'player-control.flag', 'campaign-player-kill.bin',
              'campaign-quick-actions.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox death recovery',
              'new_save': not args.no_save}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-player-kill.bin').write_bytes(struct.pack('<I', 100))
        if not args.no_save:
            (DISC / 'campaign-quick-actions.bin').write_bytes(struct.pack('<2I', 70, 0xffffffff))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay())
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 300, snapshot=True,
                          extra_symbols={'rf_scene_player_kill_test': 4,
                                         'rf_scene_endgame': 6,
                                         'rf_scene_actor_landing': 8,
                                         'rf_scene_actor_stance_flags': 1},
                          allow_guest_error=True)
        report['guest'] = guest
        expected_uid = 0xfffffffc if args.no_save else 0xfffffffd
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}, '
                               f'load stage {guest["campaign_load_stage"]}')
        if guest['level_transitions'][:2] != [1, expected_uid] or \
                guest['player_life'][2] or guest['replay_state'][2] != FRAMES:
            raise RuntimeError(f'Wrong death recovery: transitions '
                               f'{guest["level_transitions"]}, life '
                               f'{guest["player_life"]}, replay '
                               f'{guest["replay_state"]}')
        if not args.no_save and (guest['checkpoint_state'][0] != 0 or
                                 guest['checkpoint_state'][1] == 0 or
                                 guest['checkpoint_state'][4] == 0):
            raise RuntimeError(f'Saved state was not restored: {guest["checkpoint_state"]}')
        if guest['extra']['rf_scene_endgame'][0]:
            raise RuntimeError('Timeout incorrectly requested named endgame')
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
