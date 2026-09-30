"""Check a group-owned vehicle pose and Detach on stock-64-MiB Xbox.

L20S2 When_Dead18354 is process-locally fired to activate Hanger Lift001;
Detach18377 is fired at frame 60 for its masako_fighter4717 child. No
desktop input or capture is used.
"""

import datetime
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

from xemu_guest_snapshot import words
from xemu_native_world_save import FLAGS, address, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
ORIGINAL_X = 416.25457763671875


def attachment_probe(monitor, mapping):
    return words(monitor, address(mapping, 'rf_scene_passive_attachment'), 14)


def carry_probe(monitor, mapping):
    return {name: words(monitor, address(mapping, name), count)
            for name, count in (('rf_scene_passive_attachment', 14),
                                ('rf_scene_actor_pose', 59),
                                ('rf_scene_actor_landing', 8),
                                ('rf_scene_passive_roof_fixture', 10),
                                ('rf_scene_passive_side_push', 9))}


def npc_probe(monitor, mapping):
    return {name: words(monitor, address(mapping, name), count)
            for name, count in (('rf_scene_passive_attachment', 14),
                                ('rf_scene_passive_npc_fixture', 10),
                                ('rf_scene_npc_script_ground', 4),
                                ('rf_scene_npc_script_mover', 6))}


def position(row):
    return struct.unpack('<3f', struct.pack('<3I', *row[7:10]))


def actor_position(row):
    return struct.unpack('<3f', struct.pack('<3I', *row[14:17]))


def main():
    inventory = sys.argv[1:] == ['--inventory']
    submarine = sys.argv[1:] == ['--submarine']
    contact = sys.argv[1:] == ['--contact']
    npc = sys.argv[1:] == ['--npc']
    side = sys.argv[1:] == ['--side']
    rising = sys.argv[1:] == ['--rising']
    carry = sys.argv[1:] == ['--carry'] or rising
    masako = sys.argv[1:] == ['--masako'] or contact or carry or npc or side
    short = inventory or submarine or masako
    if sys.argv[1:] and not short:
        raise SystemExit('usage: xemu_vehicle_group_detach.py [--inventory|--submarine|--masako|--contact|--carry|--rising|--npc|--side]')
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / (('vehicle-group-side-' if side else
             'vehicle-group-npc-' if npc else
             'vehicle-group-rising-' if rising else
             'vehicle-group-carry-' if carry else
             'vehicle-group-contact-' if contact else
             'vehicle-group-masako-' if masako else
             'vehicle-group-submarine-' if submarine else
             'vehicle-group-inventory-' if inventory
             else 'vehicle-group-detach-') +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': 'L20S2 moving fighter side push against stationary player' if side
              else 'L20S2 scripted NPC on attached fighter4717' if npc
              else 'L20S2 group-owned masako_fighter4717' if masako
              else 'L5S3 group-owned submarine3977' if submarine
              else 'L20S1 three group-owned Fighters' if inventory
              else 'L20S2 vehicle4717 lift pose and Detach18377'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            (b'levels1.vpp' if submarine else b'levels2.vpp').ljust(64, b'\0') +
            (b'L5S3.rfl' if submarine else b'L20S1.rfl' if inventory else b'L20S2.rfl').ljust(64, b'\0'))
        if short:
            (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I',
                3977 if submarine else 1490 if inventory else 4717))
        if carry or npc or side:
            (DISC / 'campaign-passive-roof.bin').write_bytes(
                struct.pack('<3I', 4717, 40 if side else 30, 4 if side else 3 if npc else 2) if rising or npc or side
                else struct.pack('<2I', 4717, 30))
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I', 18354))
        if not short:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<2I', 18354, 18377))
        frames = 75 if side else 65 if npc else 120 if contact or carry else 10 if short else 90
        (DISC / 'player-replay.bin').write_bytes(
            b'RFI5' + struct.pack('<I', 44) +
            (struct.pack('<5f6I', 0, 0, 1, 0, 0, *([0] * 6)) * frames
             if contact else bytes(frames * 44)))
        with (folder / 'build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        guest = run_guest(folder, 'run', hdd, frames, 420, snapshot=True,
                          probe=npc_probe if npc else carry_probe if carry or side else None if short else attachment_probe,
                          probe_frame=45 if side else 40 if npc or carry else None if short else 35,
                          extra_symbols={'rf_scene_passive_attachment': 14,
                                         'rf_scene_passive_draw': 6,
                                         'rf_scene_passive_collision': 8,
                                         **({'rf_scene_passive_npc_fixture': 10,
                                             'rf_scene_npc_script_ground': 4,
                                             'rf_scene_npc_script_mover': 6} if npc else {}),
                                         **({'rf_scene_actor_pose': 59,
                                             'rf_scene_actor_landing': 8,
                                             'rf_scene_passive_roof_fixture': 10,
                                             'rf_scene_passive_rising_support': 6,
                                             'rf_scene_passive_side_push': 9} if carry or side else {})},
                          allow_guest_error=True)
        report['guest'] = guest
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'Xbox guest failed {guest["guest_phase"]:08x}')
        final = guest['extra']['rf_scene_passive_attachment']
        if inventory:
            if final[0:2] != [3, 6] or final[13]:
                raise RuntimeError(f'Three Fighter owners/six bindings absent: {final}')
            draw = guest['extra']['rf_scene_passive_draw']
            if draw[0] != 3 or draw[1] < 1 or draw[2] < 1 or draw[5]:
                raise RuntimeError(f'Fighter chassis did not submit: {draw}')
        elif submarine:
            if final[0:2] != [1, 1] or final[4] != 3977 or final[13]:
                raise RuntimeError(f'Moving submarine owner/binding absent: {final}')
            draw = guest['extra']['rf_scene_passive_draw']
            if draw[0] != 1 or draw[1] != 1 or draw[2] < 1 or draw[4] != 3977 or draw[5]:
                raise RuntimeError(f'Submarine chassis did not submit: {draw}')
        elif masako:
            if final[0:2] != [1, 1] or final[4] != 4717 or final[13]:
                raise RuntimeError(f'Masako fighter owner/binding absent: {final}')
            draw = guest['extra']['rf_scene_passive_draw']
            if draw[0] != 1 or draw[1] != 1 or draw[2] < 1 or draw[4] != 4717 or draw[5]:
                raise RuntimeError(f'Masako fighter chassis did not submit: {draw}')
            if contact and (guest['extra']['rf_scene_passive_collision'][1] < 1 or
                            guest['extra']['rf_scene_passive_collision'][4] != 4717):
                raise RuntimeError(f'Actor did not contact fighter chassis: {guest["extra"]["rf_scene_passive_collision"]}')
            if carry:
                early=guest['probe'];late=guest['extra']
                early_vehicle=position(early['rf_scene_passive_attachment'])
                late_vehicle=position(late['rf_scene_passive_attachment'])
                early_actor=actor_position(early['rf_scene_actor_pose'])
                late_actor=actor_position(late['rf_scene_actor_pose'])
                report['carry']={'early_vehicle': early_vehicle,
                                 'late_vehicle': late_vehicle,
                                 'early_actor': early_actor,
                                 'late_actor': late_actor,
                                 'early_landing': early['rf_scene_actor_landing'][1],
                                 'late_landing': late['rf_scene_actor_landing'][1],
                                 'placed': late['rf_scene_passive_roof_fixture'][2]}
                if late['rf_scene_passive_roof_fixture'][2] != 1 or \
                   (rising and late['rf_scene_passive_rising_support'][2] < 1) or \
                   abs(late_vehicle[1]-early_vehicle[1]) < .25 or \
                   early['rf_scene_actor_landing'][1] != 1 or late['rf_scene_actor_landing'][1] != 1 or \
                   any(abs((late_actor[i]-early_actor[i])-(late_vehicle[i]-early_vehicle[i])) > .75
                       for i in (0,1)):
                    raise RuntimeError(f'Actor did not ride moving fighter chassis: {report["carry"]}')
            if npc:
                early=guest['probe'];late=guest['extra']
                a=early['rf_scene_passive_npc_fixture']
                b=late['rf_scene_passive_npc_fixture']
                early_vehicle=position(early['rf_scene_passive_attachment'])
                late_vehicle=position(final)
                early_y=struct.unpack('<f',struct.pack('<I',a[3]))[0]
                late_y=struct.unpack('<f',struct.pack('<I',b[3]))[0]
                report['npc']={'early':a,'late':b,'early_vehicle':early_vehicle,
                               'late_vehicle':late_vehicle,'script_ground':late['rf_scene_npc_script_ground'],
                               'script_mover':late['rf_scene_npc_script_mover']}
                if not a[0] or a[0]!=b[0] or a[1]!=final[5] or b[1]!=final[5] or \
                   late['rf_scene_npc_script_ground'][0]<1 or \
                   late['rf_scene_npc_script_mover'][0]<2 or \
                   late['rf_scene_npc_script_mover'][1]<2 or b[8]!=1 or b[9]<1 or \
                   abs((late_y-early_y)-(late_vehicle[1]-early_vehicle[1]))>.75:
                    raise RuntimeError(f'Scripted NPC did not ride attached fighter: {report["npc"]}')
            if side:
                early=guest['probe'];late=guest['extra']
                early_vehicle=position(early['rf_scene_passive_attachment'])
                late_vehicle=position(final)
                early_actor=actor_position(early['rf_scene_actor_pose'])
                late_actor=actor_position(late['rf_scene_actor_pose'])
                report['side']={'early_vehicle':early_vehicle,'late_vehicle':late_vehicle,
                                'early_actor':early_actor,'late_actor':late_actor,
                                'push':late['rf_scene_passive_side_push']}
                actor_move=late_actor[0]-early_actor[0]
                vehicle_move=late_vehicle[0]-early_vehicle[0]
                if late['rf_scene_passive_roof_fixture'][2]!=1 or \
                   late['rf_scene_passive_side_push'][1]<1 or \
                   late['rf_scene_passive_side_push'][2]<1 or \
                   actor_move*vehicle_move<=0 or abs(actor_move)<.15 or \
                   abs(actor_move-vehicle_move)>.75 or \
                   guest['player_life'][0]:
                    raise RuntimeError(f'Fighter did not push stationary player: {report["side"]}')
        else:
            first = guest['probe']
            if first[0] < 1 or first[1] < 1 or first[2] < 1 or first[3] or first[4] != 4717:
                raise RuntimeError(f'Vehicle did not follow lift before Detach: {first}')
            if final[3] != 1 or final[4] != 4717 or final[6] != 0xffffffff:
                raise RuntimeError(f'Vehicle was not detached: {final}')
            if final[7:10] != final[10:13] or final[13]:
                raise RuntimeError(f'Vehicle did not retain its detach pose: {final}')
            for label, row in (('before', first), ('detached', final)):
                xyz = position(row)
                if not all(math.isfinite(v) for v in xyz) or xyz[0] < ORIGINAL_X + .2:
                    raise RuntimeError(f'{label} pose did not follow lift: {xyz}')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        with (folder / 'restore-build.log').open('wb') as log:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                            'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                           env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], flush=True)


if __name__ == '__main__':
    main()
