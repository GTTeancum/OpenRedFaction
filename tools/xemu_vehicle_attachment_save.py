"""Save and reload L20S2's attached or detached fighter on stock-64-MiB Xbox.

Uses only process-local campaign events and the owned ordinary-save HDD.
No desktop input, capture, PC gameplay, or campaign-route replay.
"""
import datetime
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
SYMBOLS = {'rf_scene_passive_attachment': 14, 'rf_scene_passive_draw': 6,
           'rf_scene_world_snapshot_event_probe': 6}


def build(folder, name):
    with (folder / (name + '-build.log')).open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def pose(words):
    return struct.unpack('<3f', struct.pack('<3I', *words[7:10]))


def actor_pose(words):
    return struct.unpack('<3f', struct.pack('<3I', *words[14:17]))


def main():
    attached_mode = sys.argv[1:] == ['--attached']
    riding_mode = sys.argv[1:] == ['--riding']
    dead_mode = sys.argv[1:] == ['--dead']
    if sys.argv[1:] and not (attached_mode or riding_mode or dead_mode):
        raise SystemExit('usage: xemu_vehicle_attachment_save.py [--attached|--riding|--dead]')
    linked_mode = attached_mode or riding_mode or dead_mode
    symbols = dict(SYMBOLS)
    if dead_mode:
        symbols.update(rf_scene_watch_links=40,rf_scene_death_watches=97,
                       rf_scene_passive_damage=8,rf_scene_watch_test=4,
                       rf_scene_watch_ray=6)
    if riding_mode:
        symbols.update(rf_scene_actor_pose=59, rf_scene_actor_landing=8,
                       rf_scene_passive_rising_support=6,
                       rf_scene_world_player_probe=3,
                       campaign_support_handle=1,campaign_support_velocity=3)
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare(ROOT, base)
    folder = ROOT / 'artifacts/xemu' / (('vehicle-death-save-' if dead_mode else
             'vehicle-riding-save-' if riding_mode else
             'vehicle-attached-save-' if attached_mode
             else 'vehicle-attachment-save-') +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {'campaign-watch.bin'} | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    report = {'result': 'FAIL', 'scope': ('L20S2 two dead Fighters ordinary save/reload'
              if dead_mode else 'L20S2 rider on attached fighter ordinary save/reload'
              if riding_mode else 'L20S2 attached fighter ordinary save/reload'
              if attached_mode else 'L20S2 group-owned fighter detach ordinary save/reload'),
              'phases': {}}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels2.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'))
        if dead_mode:
            (DISC / 'campaign-watch.bin').write_bytes(struct.pack('<I',18354))
        else:
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<I' if linked_mode else '<2I',
                                                                 *((18354,) if linked_mode else (18354, 18377))))
        if riding_mode:
            (DISC / 'campaign-passive-roof.bin').write_bytes(struct.pack('<3I', 4717, 30, 2))
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        save_frames = 90 if riding_mode or dead_mode else 40 if attached_mode else 90
        load_frames = 20 if linked_mode else 10
        (DISC / 'player-replay.bin').write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(save_frames * 44))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, save_frames, 420, capture_world=True,
                          extra_symbols=symbols, allow_guest_error=True)
        report['phases']['save'] = saved
        state = saved['checkpoint_state']
        if state[9] != 1 or state[3] or state[4] < 320:
            raise RuntimeError(f'Xbox save failed: {state}, event probe '
                               f'{saved["extra"]["rf_scene_world_snapshot_event_probe"]}')
        if saved['extra']['rf_scene_passive_attachment'][3] != (0 if linked_mode else 1):
            raise RuntimeError('Fighter attachment state was wrong before save')
        if dead_mode:
            watch=saved['extra']['rf_scene_death_watches']
            rows=[watch[i:i+3] for i in range(1,1+watch[0]*3,3)]
            death=next((row for row in rows if row[0]==18354),None)
            if not death or death[1]!=1 or saved['extra']['rf_scene_passive_damage'][3]!=2 or \
               saved['extra']['rf_scene_watch_test']!=[2,18353,60,0] or \
               saved['extra']['rf_scene_watch_ray'][1]!=2:
                raise RuntimeError(f'Fighters did not die before save: {death}')
        if riding_mode:
            if saved['extra']['rf_scene_actor_landing'][1] != 1 or \
               saved['extra']['rf_scene_passive_rising_support'][2] < 1:
                raise RuntimeError('Player was not riding before save')
        payload = (folder / 'save/xbox-world.rfwc').read_bytes()
        directory = 128 + (11 - 1) * 12
        offset, length = struct.unpack_from('<II', payload, directory + 4)
        vehicle = payload[offset:offset + length]
        if vehicle[:4] != b'RFVA':
            raise RuntimeError('Vehicle attachment section is absent')
        version, host_bytes, count = struct.unpack_from('<3I', vehicle, 4)
        if version != 2 or count != 8 or len(vehicle) != 16 + host_bytes + count * 80:
            raise RuntimeError('Vehicle attachment section is malformed')
        rows = [(struct.unpack_from('<2I', vehicle, 16 + host_bytes + index * 80),
                 struct.unpack_from('<3f', vehicle, 24 + host_bytes + index * 80),
                 struct.unpack_from('<2f', vehicle, 72 + host_bytes + index * 80),
                 struct.unpack_from('<I', vehicle, 92 + host_bytes + index * 80)[0])
                for index in range(count)]
        matches = [(identity[1], point) for identity, point, _, _ in rows if identity[0] == 4717]
        if len(matches) != 1:
            raise RuntimeError(f'Expected one authored fighter4717 row, found {len(matches)}')
        attached, saved_pose = matches[0]
        uid = 4717
        if uid != 4717 or attached != int(linked_mode):
            raise RuntimeError(f'Vehicle attachment state not saved: {uid}, {attached}')
        report['saved_pose'] = saved_pose
        if dead_mode:
            dead_rows={identity[0]:(vitals[0],destroyed)
                       for identity, _, vitals, destroyed in rows if identity[0] in (4801,18353)}
            report['dead_rows']=dead_rows
            if len(dead_rows)!=2 or any(health>0 or destroyed!=1
                                        for health,destroyed in dead_rows.values()):
                raise RuntimeError(f'Fighter death state not saved: {dead_rows}')
        if riding_mode:
            environment_row = 128 + (16 - 1) * 12
            env_offset, env_length = struct.unpack_from('<II', payload, environment_row + 4)
            support_uid = struct.unpack_from('<I', payload, env_offset + 76)[0]
            if env_length < 80 or support_uid != 4717:
                raise RuntimeError(f'Rider support UID not saved: {support_uid}')
            report['saved_actor'] = actor_pose(saved['extra']['rf_scene_actor_pose'])
        (DISC / 'campaign-setup.bin').unlink(missing_ok=True)
        if riding_mode:(DISC / 'campaign-passive-roof.bin').unlink()
        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(b'RFI5' + struct.pack('<I', 44) + bytes(load_frames * 44))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, load_frames, 420, snapshot=True,
                           extra_symbols=symbols)
        report['phases']['load'] = loaded
        loaded_state = loaded['checkpoint_state']
        attachment = loaded['extra']['rf_scene_passive_attachment']
        if loaded_state[8] != 1 or loaded_state[0] or loaded_state[1] != state[4]:
            raise RuntimeError(f'Xbox load failed: {loaded_state}')
        if attachment[3] != (0 if linked_mode else 1) or attachment[4] != 4717:
            raise RuntimeError(f'Attachment not restored: {attachment}')
        if dead_mode:
            watch=loaded['extra']['rf_scene_death_watches']
            rows=[watch[i:i+3] for i in range(1,1+watch[0]*3,3)]
            death=next((row for row in rows if row[0]==18354),None)
            links=loaded['extra']['rf_scene_watch_links']
            report['loaded_death']={'watch':death,'links':links[:15]}
            if not death or death[1]!=1 or links[0]!=18354 or \
               links[7]!=0 or links[8]!=2 or links[13]!=0 or links[14]!=2 or \
               loaded['extra']['rf_scene_passive_damage'][3]:
                raise RuntimeError(f'Fighters revived or watcher refired: {report["loaded_death"]}')
        if linked_mode:
            distance = sum((a - b) ** 2 for a, b in zip(pose(attachment), saved_pose)) ** .5
            if attachment[2] < 1 or not .01 < distance < 10:
                raise RuntimeError(f'Attached child did not continue smoothly: '
                                   f'{pose(attachment)} vs {saved_pose}, moves {attachment[2]}')
            if riding_mode:
                loaded_actor=actor_pose(loaded['extra']['rf_scene_actor_pose'])
                report['loaded_actor']=loaded_actor
                if loaded['extra']['rf_scene_actor_landing'][1] != 1 or \
                   any(abs((loaded_actor[i]-report['saved_actor'][i])-
                           (pose(attachment)[i]-saved_pose[i]))>.75 for i in (0,1)):
                    raise RuntimeError(f'Rider did not continue with attached fighter: '
                                       f'{report["saved_actor"]} -> {loaded_actor}, '
                                       f'{saved_pose} -> {pose(attachment)}')
        elif any(abs(a - b) > 0.00001 for a, b in zip(pose(attachment), saved_pose)):
            raise RuntimeError(f'Detached world pose changed: {pose(attachment)} vs {saved_pose}')
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
