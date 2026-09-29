"""Bounded Xbox NPC death/corpse and idle landing-impact checks."""
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
FRAMES = 90


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    drops = parser.add_mutually_exclusive_group()
    drops.add_argument('--impact-drop', action='store_true',
                        help='Drive a real NPC into a damaging landing instead of scripted Slay')
    drops.add_argument('--scripted-drop', action='store_true',
                       help='Give an L1S1 authored Goto actor a damaging nonlethal landing')
    parser.add_argument('--scripted-speed', type=float, choices=(11.0, 20.0), default=11.0,
                        help='Downward fixture speed for scripted-drop diagnosis')
    args = parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    prefix = 'npc-scripted-drop-' if args.scripted_drop else 'npc-impact-drop-' if args.impact_drop else 'npc-death-audio-'
    folder = ROOT / 'artifacts/xemu' / (prefix +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.add('player-control.flag')
    names.add('campaign-npc-drop.bin')
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    scripted_lethal = args.scripted_drop and args.scripted_speed >= 20
    report = {'result': 'FAIL', 'scope': 'Xbox authored Goto actor lethal landing' if scripted_lethal
              else 'Xbox authored Goto actor damaging landing' if args.scripted_drop
              else 'Xbox synthetic high-speed NPC landing through live scene' if args.impact_drop
              else 'Xbox authored NPC death, owned corpse and idle landing impact'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 8432))
        if args.scripted_drop:
            (DISC / 'campaign-goto.bin').write_bytes(struct.pack('<2I', 9363, 30))
            (DISC / 'campaign-npc-drop.bin').write_bytes(struct.pack('<IfI', 8432, args.scripted_speed, 31))
        elif args.impact_drop:
            (DISC / 'campaign-npc-drop.bin').write_bytes(struct.pack('<If', 8432, 20.0))
        else:
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
                                         'rf_scene_npc_idle_ground': 6,
                                         'rf_scene_npc_idle_impact': 5,
                                         'rf_scene_npc_script_ground': 4,
                                         'rf_scene_script_movement': 8,
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
        idle_ground = guest['extra']['rf_scene_npc_idle_ground']
        idle_impact = guest['extra']['rf_scene_npc_idle_impact']
        script_ground = guest['extra']['rf_scene_npc_script_ground']
        if guest['replay_state'][2] != FRAMES:
            raise RuntimeError(f'Xbox replay did not complete: {guest["replay_state"]}')
        if args.scripted_drop:
            if (any(slay) or not script_ground[0] or script_ground[3] != 1 or
                    idle_impact[2] != int(scripted_lethal) or idle_impact[1] != int(not scripted_lethal) or
                    idle_impact[3] != int(scripted_lethal)):
                raise RuntimeError(f'Scripted landing did not apply expected damage: {script_ground}, {idle_impact}, {slay}')
        elif args.impact_drop:
            if any(slay) or idle_impact[2] != 1 or idle_impact[3] != 1 or not idle_impact[0]:
                raise RuntimeError(f'Damaging landing did not kill with impact sound: {idle_impact}, {slay}')
        elif slay[:3] != [1, 1, 8432] or slay[5]:
            raise RuntimeError(f'Authored death did not complete: {slay}')
        if args.scripted_drop and not scripted_lethal:
            if any(death) or any(selection) or any(corpses):
                raise RuntimeError(f'Nonlethal landing unexpectedly started death: {death}, {selection}, {corpses}')
        else:
            if death != [1, 1, 0, 0]:
                raise RuntimeError(f'Death action did not start one voice: {death}')
            if selection[:3] != [1, 1, 0] or selection[3] >= 45:
                raise RuntimeError(f'Live death did not select an authored action: {selection}')
            if corpses[0] != 1 or corpses[1] != 1 or not corpses[2] or not corpses[3] or not corpses[4] or any(corpses[5:]):
                raise RuntimeError(f'Owned corpse was not updated and drawn: {corpses}')
        if args.scripted_drop:
            if idle_ground[4] or idle_impact[4] or idle_impact[0] < script_ground[3]:
                raise RuntimeError(f'Scripted NPC ground pass failed: {script_ground}, {idle_impact}')
        else:
            if not all(idle_ground[:4]) or idle_ground[4]:
                raise RuntimeError(f'Idle NPC ground pass did not run cleanly: {idle_ground}')
            if idle_impact[0] != idle_ground[3] + idle_impact[2] or idle_impact[4]:
                raise RuntimeError(f'Idle NPC landings did not dispatch impact: {idle_impact}')
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
