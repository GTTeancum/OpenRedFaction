"""Bounded Xbox NPC death-action audio check with an authored Slay event."""
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
FRAMES = 90


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
    folder = ROOT / 'artifacts/xemu' / ('npc-death-audio-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'Xbox authored Slay to NPC death action audio and owned corpse'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 8432))
        (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 9362, 9362))
        (DISC / 'player-control.flag').write_bytes(b'')
        neutral = struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI6' + struct.pack('<I', 48) + neutral * FRAMES)
        build(folder, 'run')
        guest = run_guest(folder, 'run', hdd, FRAMES, 240, snapshot=True,
                          extra_symbols={'rf_scene_script_slays': 6,
                                         'rf_scene_live_death_audio': 4,
                                         'rf_scene_live_death_selection': 4,
                                         'rf_scene_live_corpses': 8,
                                         'rf_scene_npc_action_audio': 9,
                                         'rf_scene_combat_death': 8},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed: {guest["guest_phase"]:08x}')
        slay = guest['extra']['rf_scene_script_slays']
        death = guest['extra']['rf_scene_live_death_audio']
        selection = guest['extra']['rf_scene_live_death_selection']
        corpses = guest['extra']['rf_scene_live_corpses']
        if slay[:3] != [1, 1, 8432] or slay[5] or guest['replay_state'][2] != FRAMES:
            raise RuntimeError(f'Authored death did not complete: {slay}')
        if death != [1, 1, 0, 0]:
            raise RuntimeError(f'Death action did not start one voice: {death}')
        if selection[:3] != [1, 1, 0] or selection[3] >= 45:
            raise RuntimeError(f'Live death did not select an authored action: {selection}')
        if corpses[0] != 1 or corpses[1] != 1 or not corpses[2] or not corpses[3] or not corpses[4] or any(corpses[5:]):
            raise RuntimeError(f'Owned corpse was not updated and drawn: {corpses}')
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
