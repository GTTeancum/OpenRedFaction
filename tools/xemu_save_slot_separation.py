"""Check manual and section-autosave isolation in one stock-64-MiB Xbox run.

The guest saves L1S1 manually, crosses one authored exit, autosaves L1S2,
then explicitly quickloads the still-intact L1S1 manual save. All input is
process-local; no campaign route, host input, or user HDD is involved.
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
FRAMES = 220


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
    folder = ROOT / 'artifacts/xemu' / ('save-slot-separation-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'campaign-quick-actions.bin', 'player-control.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Xbox manual save survives section autosave'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-exit.bin').write_bytes(struct.pack('<I', 9019))
        (DISC / 'campaign-quick-actions.bin').write_bytes(struct.pack('<2I', 55, 150))
        (DISC / 'player-control.flag').write_bytes(b'')
        rows = [struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
                for _ in range(FRAMES)]
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + b''.join(rows))
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 360, snapshot=True,
                          extra_symbols={'rf_scene_section_autosave': 4,
                                         'campaign_current_level': 16},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}')
        autosave = guest['extra']['rf_scene_section_autosave']
        level_words = guest['extra']['campaign_current_level']
        level = struct.pack('<16I', *level_words).split(b'\0', 1)[0].decode('ascii')
        report['final_level'] = level
        if autosave[:3] != [1, 1, 0] or not 30 <= autosave[3] <= 180:
            raise RuntimeError(f'No valid section autosave: {autosave}')
        if guest['level_transitions'][:2] != [2, 0xfffffffd] or level != 'L1S1.rfl':
            raise RuntimeError(f'Manual save not restored: {guest["level_transitions"]}, {level}')
        if guest['checkpoint_state'][0] != 0 or guest['checkpoint_state'][1] == 0 or \
                guest['replay_state'][2] != FRAMES:
            raise RuntimeError(f'Incomplete restore: {guest["checkpoint_state"]}, '
                               f'{guest["replay_state"]}')
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
