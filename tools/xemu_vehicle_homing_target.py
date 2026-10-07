"""Private bounded vehicle-homing fixture; no framebuffer or host input."""
import datetime
import io
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess

from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_vehicle_fighter_switch import prepare_level
from xemu_native_world_save import FLAGS, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_host import xbox_build_command

ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
HDD = ROOT / 'local/xemu-harness/pacing-base.qcow2'

def replay(kind, frames):
    rows = []
    for frame in range(frames):
        pitch = 0.0
        yaw = 0.0
        fire = int(kind == 'submarine' and frame == 90)
        alt = int(kind == 'fighter' and frame == 140)
        rows.append(struct.pack('<5f7I', 0, 0, 0, pitch, yaw,
                                0, 0, int(frame == (90 if kind == 'fighter' else 12)), fire, 0, 0, alt))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)

def submarine_level(folder):
    original = read_entry(ROOT / 'Installed_Game/levels1.vpp', 'L5S3.rfl')
    meta = inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='L5S3.rfl'))
    section = next(s for s in meta['sections'] if s['type'] == '0x30000')
    row = next(r for r in entity_rows(original) if r['uid'] == 8163)
    active = next(r for r in entity_rows(original) if r['uid'] == 3963)
    x, y, z = struct.unpack_from('<3f', active['raw'], active['transform'])
    # The authored active submarine faces approximately +Z. Keep an original
    # ungrouped submarine owner and move only its fixture transform.
    position = (x + .86, y, z + 5.94)
    patched = bytearray(original)
    at = row['offset'] + row['transform']
    struct.pack_into('<3f', patched, at, *position)
    check = next(r for r in entity_rows(patched) if r['uid'] == 8163)
    if math.dist(struct.unpack_from('<3f', check['raw'], check['transform']), position) > .001:
        raise RuntimeError('Submarine fixture transform did not round trip')
    path = folder / 'levels1-target.vpp'
    archive(path, [('L5S3.rfl', patched)])
    return path, position

def fighter_level(folder):
    prepared, info = prepare_level(folder)
    original = read_entry(prepared, 'L13S3.rfl')
    meta = inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='L13S3.rfl'))
    section = next(s for s in meta['sections'] if s['type'] == '0x30000')
    rows = {r['uid']: r for r in entity_rows(original)}
    row = rows[7629]
    raw = row['raw']
    name_at = row['transform'] + 48
    friendship = name_at + 2 + struct.unpack_from('<H', raw, name_at)[0] + 5
    if struct.unpack_from('<I', raw, friendship)[0] != 1:
        raise RuntimeError('Expected original friendly Jeep fixture')
    patched = bytearray(original)
    struct.pack_into('<I', patched, row['offset'] + friendship, 0)
    fighter = rows[8955]
    fighter_at = fighter['offset'] + fighter['transform']
    jeep_at = row['offset'] + row['transform']
    struct.pack_into('<3f', patched, fighter_at, 35.5, 6.38, -140.0)
    struct.pack_into('<9f', patched, fighter_at + 12, 0, 0, 1, 1, 0, 0, 0, 1, 0)
    struct.pack_into('<3f', patched, jeep_at, 36.2, 4.719160313, -119.0)
    start = next(s for s in meta['sections'] if s['type'] == '0x70000')
    struct.pack_into('<3f', patched, start['offset'] + 8, 35.5, 6.7013111, -144.0)
    check = next(r for r in entity_rows(patched) if r['uid'] == 7629)
    if struct.unpack_from('<I', check['raw'], friendship)[0] != 0:
        raise RuntimeError('Hostile Jeep fixture did not round trip')
    path = folder / 'levelsm-target.vpp'
    archive(path, [('L13S3.rfl', patched)])
    info['homing_stage'] = {'fighter_position': [35.5, 6.38, -140],
                            'jeep_position': [36.2, 4.719160313, -119],
                            'spawn': [35.5, 6.7013111, -144],
                            'geometry_room': 0, 'floor_faces': [6080, 6081],
                            'floor_y': 4, 'room_collision_skipped': False,
                            'floor_corridor_bounds': [[29.25, 41.3125], [-149.625, -100]],
                            'reason': '21m shot permits unchanged 0.25s wakeup at25m/s and avoids15m pilot blast',
                            'fighter_orientation': 'identity, forward+Z', 'jeep_affiliation': 0}
    for owner in info['owners']:
        point = info['homing_stage']['fighter_position' if owner['uid'] == 8955 else 'jeep_position']
        owner['position'] = point
        owner['world_spheres'] = [dict(center=[point[k]+sphere['center'][k] for k in range(3)], radius=sphere['radius'])
                                  for sphere in owner['model']['spheres']]
        owner['bounds'] = [[min(v['center'][k]-v['radius'] for v in owner['world_spheres']),
                           max(v['center'][k]+v['radius'] for v in owner['world_spheres'])] for k in range(3)]
    info['spawn'] = info['homing_stage']['spawn']
    info['geometry'] = dict(source='levelsm.vpp/ctf06.rfl', unchanged=True,
                            room=0, floor_y=4, floor_faces=[6080, 6081])
    info['frames'] = 300
    info['replay'] = dict(board_Fighter=90, rocket=140, pre_health_probe=1, post_health_probe=220)
    info['limitations'] = 'Bounded authored-owner testbed only; no campaign route, save continuation or visual claim.'
    (folder / 'recipe.json').write_text(json.dumps(info, indent=2) + '\n')
    return path, info

def read_owner(monitor, mapping, target_uid):
    layout = words(monitor, address(mapping, 'rf_scene_vehicle_homing_owner_layout'), 4)
    stride, handle_offset, uid_offset, health_offset = layout
    count = words(monitor, address(mapping, 'campaign_passive_vehicle_count'), 1)[0]
    pointer = words(monitor, address(mapping, 'campaign_passive_vehicles'), 1)[0]
    if not pointer or not 0 < count <= 1024 or not 0 < stride <= 16384:
        raise RuntimeError('Invalid passive owner telemetry layout')
    found = []
    for i in range(count):
        at = pointer + i * stride
        uid = words(monitor, at + uid_offset, 1)[0]
        if uid == target_uid:
            handle = words(monitor, at + handle_offset, 1)[0]
            bits = words(monitor, at + health_offset, 1)[0]
            found.append({'uid': uid, 'handle': handle,
                          'health': struct.unpack('<f', struct.pack('<I', bits))[0]})
    if len(found) != 1:
        raise RuntimeError(f'Expected exactly one owner for UID{target_uid}, found {len(found)}')
    found[0]['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return found[0]


def repack(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(xbox_build_command('--repack'), cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'),
                       stdout=log, stderr=subprocess.STDOUT, check=True)

def run_case(kind):
    require_no_project_xemu(ROOT)
    folder = ROOT / 'artifacts/xemu' / ('vehicle-homing-' + kind + '-' +
             datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    archive_name = 'levelsm.vpp' if kind == 'fighter' else 'levels1.vpp'
    backed = folder / ('original-' + archive_name)
    shutil.copyfile(DISC / archive_name, backed)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names.update(('player-control.flag', 'scene-preview.flag'))
    originals = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                 for name in names}
    report = {'result': 'FAIL', 'kind': kind}
    try:
        if kind == 'fighter':
            staged, info = fighter_level(folder)
            report['fixture'] = info
            uid, level, frames = 8955, 'L13S3.rfl', 300
        else:
            staged, position = submarine_level(folder)
            report['target_position'] = position
            uid, level, frames = 3963, 'L5S3.rfl', 240
        shutil.copyfile(staged, DISC / archive_name)
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(archive_name.encode().ljust(64, b'\0') + level.encode().ljust(64, b'\0'))
        if kind == 'submarine':
            (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', uid))
        (DISC / 'scene-preview.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        replay_bytes = replay(kind, frames)
        (DISC / 'player-replay.bin').write_bytes(replay_bytes)
        (folder / 'player-replay.bin').write_bytes(replay_bytes)
        repack(folder, 'run')
        listing = subprocess.check_output([str(Path(os.environ['NXDK_DIR']) / 'tools/extract-xiso/build' / ('extract-xiso.exe' if os.name == 'nt' else 'extract-xiso')),
            '-l', str(ROOT / 'build/xbox/redfaction-diagnostic.iso')], text=True)
        (folder / 'run-iso-list.txt').write_text(listing)
        if 'player-replay.bin' not in listing or archive_name not in listing:
            raise RuntimeError('Repacked ISO omitted the process-local replay or level archive')
        extra = {'rf_scene_vehicle_enabled': 1, 'rf_scene_vehicle_state': 16,
                 'rf_scene_passive_damage': 8,
                 'rf_scene_fighter_weapon': 8, 'rf_scene_fighter_homing_state': 4,
                 'rf_scene_fighter_candidate_probe': 10, 'rf_scene_fighter_aim_probe': 3,
                 'rf_scene_vehicle_entry_probe': 16} if kind == 'fighter' else {
                 'rf_scene_vehicle_enabled': 1, 'rf_scene_vehicle_state': 16,
                 'rf_scene_passive_damage': 8, 'rf_scene_submarine_weapon': 8,
                 'rf_scene_submarine_homing_state': 8, 'rf_scene_submarine_impact_state': 10}
        target_uid = 7629 if kind == 'fighter' else 8163
        owner_probe = lambda monitor, mapping: read_owner(monitor, mapping, target_uid)
        guest = run_guest(folder, 'run', HDD, frames, 240, snapshot=True,
                          extra_symbols=extra, allow_guest_error=True,
                          probe=owner_probe, probe_frame=1,
                          final_probe=owner_probe, final_probe_frame=220 if kind == 'fighter' else 160)
        report['guest'] = guest
        values = guest['extra']
        if guest['guest_phase'] & 0x80000000:
            raise RuntimeError(f'guest failed {guest["guest_phase"]:08x}')
        if values['rf_scene_vehicle_state'][3] != 1:
            raise RuntimeError('vehicle was not boarded')
        homing = values['rf_scene_fighter_homing_state' if kind == 'fighter' else 'rf_scene_submarine_homing_state']
        weapon = values['rf_scene_fighter_weapon' if kind == 'fighter' else 'rf_scene_submarine_weapon']
        before, after = guest['probe'], guest['final_probe']
        if before['frame'] >= (140 if kind == 'fighter' else 90):
            raise RuntimeError(f'missed pre-fire health sample: {before}')
        if before['uid'] != target_uid or before['handle'] != after['handle'] or homing[2] != before['handle']:
            raise RuntimeError(f'lock did not retain intended vehicle owner: {homing}, {before}, {after}')
        if not math.isfinite(after['health']) or not 0 <= after['health'] < before['health']:
            raise RuntimeError(f'vehicle health did not fall: {before}, {after}')
        report['target_owner'] = {'before': before, 'after': after}
        if not homing[1] or not homing[3]:
            raise RuntimeError(f'no vehicle lock/turn: {homing}')
        if values['rf_scene_passive_damage'][4] != target_uid or not values['rf_scene_passive_damage'][2]:
            raise RuntimeError(f'no vehicle damage: {values["rf_scene_passive_damage"]}')
        if kind == 'fighter' and (not weapon[2] or not weapon[4]):
            raise RuntimeError(f'fighter rocket did not launch/contact: {weapon}')
        if kind == 'submarine' and (not weapon[1] or not weapon[2] or not weapon[6]):
            raise RuntimeError(f'torpedo did not launch/contact/detonate: {weapon}')
        report['result'] = 'PASS'
    except Exception as exc:
        report['error'] = repr(exc)
    finally:
        shutil.copyfile(backed, DISC / archive_name)
        for name, data in originals.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        repack(folder, 'restore')
        report['restored'] = all((DISC / name).read_bytes() == data if data is not None else not (DISC / name).exists()
                                 for name, data in originals.items())
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(folder, report['result'], report.get('error', ''), flush=True)
    return report

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 2 or sys.argv[1] not in ('fighter', 'submarine'):
        raise SystemExit('usage: xemu_vehicle_homing_target.py fighter|submarine')
    if run_case(sys.argv[1])['result'] != 'PASS':
        raise SystemExit(1)
