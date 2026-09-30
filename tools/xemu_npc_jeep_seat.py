"""Bounded Xbox authored Jeep/miner seat and driver-death route inhibition.

Uses enemy-free CTF06 geometry under the L12S1 entry name needed by the existing
Jeep selector. Complete miner7646/Jeep7629 records retain their authored link,
vitals and loadout; only transforms change. Normal process-local setup events
start a synthetic one-node route at frame0 and Slay its driver at frame60.
No desktop input, screenshots, original runtime or campaign traversal.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry, U, F, S
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from inspect_models import inspect as inspect_model
from inspect_navigation_records import inspect as inspect_navigation
from xemu_turret_combat import entity_rows, f
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR, HOST = 7646, 7629
ROUTE, SLAY, NODE = 910940, 910941, 910942
FRAMES, DEATH_FRAME = 180, 60
IDENTITY_DISK = (0, 0, 1, 1, 0, 0, 0, 1, 0)
SYMBOLS = {'rf_scene_npc_seats': 10, 'rf_scene_npc_seat_probe': 12,
           'rf_scene_vehicle_state': 16, 'rf_scene_vehicle_route_state': 8,
           'rf_scene_setup_result': 4, 'rf_scene_script_slays': 6,
           'rf_scene_enemy_combat': 8, 'rf_scene_vehicle_damage': 8,
           'rf_scene_turret_operator': 8}


def jeep_seat():
    """Independent installed static-model tag metadata, not a guessed offset."""
    data = read_entry(ROOT/'Installed_Game/meshes.vpp', 'Jeep01.v3m')
    lod = next(s for s in inspect_model(data)['sections'] if s.get('lods'))['lods'][0]
    for index in range(lod['props']):
        at = lod['attachment_offset'] + index*100
        if data[at:at+68].split(b'\0', 1)[0] == b'interface_1':
            parent = struct.unpack_from('<i', data, at+96)[0]
            if parent != -1:
                raise ValueError('Jeep interface is not an unparented static tag')
            point = struct.unpack_from('<3f', data, at+84)
            if not all(math.isfinite(v) for v in point):
                raise ValueError('Invalid Jeep seat offset')
            return dict(index=index, position=point, model_sha256=hashlib.sha256(data).hexdigest())
    raise ValueError('Jeep interface_1 missing')


def prepare_level(folder):
    source = read_entry(ROOT/'Installed_Game/levels2.vpp', 'L12S1.rfl')
    originals = entity_rows(source)
    pair = [next(r for r in originals if r['uid'] == uid) for uid in (HOST, ACTOR)]
    if pair[0]['name'] != 'Jeep01' or pair[1]['name'].lower() != 'miner1':
        raise ValueError(f'Unexpected authored pair classes: {[r["name"] for r in pair]}')
    details = record_details(pair[1])
    if details['seat_host_uid'] != HOST:
        raise ValueError('Original miner does not reference Jeep7629')
    original = read_entry(ROOT/'Installed_Game/levelsm.vpp', 'ctf06.rfl')
    if entity_rows(original):
        raise ValueError('CTF06 no longer provides an empty entity testbed')
    meta = inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='ctf06.rfl'))
    start = next(s for s in meta['sections'] if s['type'] == '0x70000')
    spawn = struct.unpack_from('<3f', original, start['offset']+8)
    # Explicit placement fixture; ordinary rigid support owns settling.
    host_position = (spawn[0], spawn[1]+.8, spawn[2]+6)
    seat = jeep_seat()
    actor_position = tuple(host_position[i]+seat['position'][i] for i in range(3))
    copied = []
    for row, point in zip(pair, (host_position, actor_position)):
        raw = bytearray(row['raw']); at = row['transform']
        raw[at:at+48] = F(*point, *IDENTITY_DISK)
        if raw[:at] != row['raw'][:at] or raw[at+48:] != row['raw'][at+48:]:
            raise ValueError('Modified non-transform authored entity bytes')
        copied.append(raw)
    # One distant point keeps the route active throughout the short command
    # check. Neither route completion nor physical travel distance is asserted.
    target = (host_position[0], host_position[1], host_position[2]+100)
    nav = U(1, NODE)+b'\1'+F(2, *target, 1)+U(0)+bytes(4)+F(0)+U(0)+b'\0'
    nodes = inspect_navigation(nav)
    if len(nodes) != 1 or nodes[0]['uid'] != NODE:
        raise ValueError('Synthetic navigation record did not decode')
    path_name = b'jeep_seat_route'
    # Reuse the existing event encoder; replace only its empty second text.
    follow = event(ROUTE, 'Follow_Waypoints', 'seat_route', (HOST,), text=path_name.decode())
    suffix = S(path_name)+S(b'')+U(1, HOST)+bytes([255]*4)
    if not follow.endswith(suffix):
        raise ValueError('Event helper layout changed')
    follow = follow[:-len(suffix)]+S(path_name)+S(b'One way')+U(1, HOST)+bytes([255]*4)
    slay = event(SLAY, 'Slay_Object', 'seat_driver_death', (ACTOR,))
    replacements = {0x30000: U(2)+b''.join(copied), 0x600: U(2)+follow+slay,
                    0x60000: U(0), 0x20000: nav, 0x10000: U(1)+S(path_name)+U(1, 0)}
    out = bytearray(original[:meta['sections'][0]['offset']]); offsets = {}; added = 0
    present = {int(s['type'], 16) for s in meta['sections']}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        if kind == 0:
            for missing in sorted(replacements.keys()-present):
                payload = replacements[missing]; offsets[missing] = len(out)
                out += U(missing, len(payload))+payload; added += 1
        payload = replacements.get(kind, original[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind] = len(out); out += U(kind, len(payload))+payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    struct.pack_into('<I', out, 20, meta['declared_sections']+added)
    inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L12S1.rfl'))
    check = entity_rows(out)
    if [r['uid'] for r in check] != [HOST, ACTOR] or record_details(check[1]) != details:
        raise ValueError('Authored pair failed fixture round trip')
    folder.mkdir(parents=True, exist_ok=True)
    path = folder/'scene-fixture.vpp'; archive(path, [('L12S1.rfl', out)])
    recipe = dict(source='levels2.vpp/L12S1.rfl', geometry='levelsm.vpp/ctf06.rfl',
                  entry_alias='L12S1.rfl', host_position=host_position,
                  actor_initial_position=actor_position, actor_details=details, seat=seat,
                  staged_fields=['entity transforms', 'synthetic navigation/route/Slay events'],
                  setup_frames={str(ROUTE): 0, str(SLAY): DEATH_FRAME}, frames=FRAMES,
                  records=[dict(uid=r['uid'], class_name=r['name'], bytes=len(r['raw']),
                                sha256=hashlib.sha256(r['raw']).hexdigest()) for r in pair])
    (folder/'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    return path, recipe


def live_probe(monitor, mapping):
    result = {name: words(monitor, address(mapping, name), count) for name, count in SYMBOLS.items()}
    result['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return result


def validate(result, recipe, link_only):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or result['memory_bytes'] != 64*1024*1024:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live, final = result['probe'], result['extra']
    seat, pose, vehicle = live['rf_scene_npc_seats'], live['rf_scene_npc_seat_probe'], live['rf_scene_vehicle_state']
    if not 15 <= live['frame'] < DEATH_FRAME:
        raise RuntimeError('Live probe missed pre-death window; no inferred live-state pass')
    if seat[:4] != [1, 1, 0, 0] or seat[4] < 2 or seat[5:7] != [0, 1] or seat[7:] != [ACTOR, HOST, 0]:
        raise RuntimeError(f'Authored driver seat failed: {seat}')
    if pose[:2] != [ACTOR, HOST] or pose[2] == pose[3] or 0xffffffff in pose[2:4] or pose[6] != 13 or pose[7] != pose[3] or pose[11] != recipe['seat']['index']:
        raise RuntimeError(f'Wrong actor/host/action/tag linkage: {pose}')
    if f(pose[4]) <= 0 or vehicle[3] or vehicle[5] or vehicle[12] != pose[3]:
        raise RuntimeError('NPC driver did not own the live Jeep independently of player session')
    if not all(math.isfinite(f(x)) for x in pose[4:6]+pose[8:11]+vehicle[6:12]):
        raise RuntimeError('Nonfinite live vitals/pose')
    for snapshot in (live, final):
        if snapshot['rf_scene_vehicle_state'][5] or snapshot['rf_scene_npc_seats'][9] or snapshot['rf_scene_vehicle_route_state'][7] or snapshot['rf_scene_enemy_combat'][7]:
            raise RuntimeError('Vehicle, seating or NPC runtime error')
        if snapshot['rf_scene_vehicle_state'][3] or snapshot['rf_scene_enemy_combat'][2] or snapshot['rf_scene_turret_operator'][0]:
            raise RuntimeError('Unexpected player possession, handheld fire or turret operator')
    route = final['rf_scene_vehicle_route_state']; slay = final['rf_scene_script_slays']
    if live['rf_scene_vehicle_route_state'][3] < 1 or route[0] != 1 or route[5] != ROUTE or route[6] != pose[3]:
        raise RuntimeError('Expected retained synthetic route was not driving')
    if link_only:
        if final['rf_scene_npc_seats'][5:7] != [0, 1] or any(slay) or route[3] <= DEATH_FRAME:
            raise RuntimeError('Living driver failed to retain seat and drive')
        # Completed telemetry is stable. A rigid rotation preserves tag offset
        # length, so this checks model-derived placement without reading C ABI.
        end_pose, end_host = final['rf_scene_npc_seat_probe'], final['rf_scene_vehicle_state']
        distance = math.sqrt(sum((f(end_pose[8+i])-f(end_host[6+i]))**2 for i in range(3)))
        expected = math.sqrt(sum(v*v for v in recipe['seat']['position']))
        if abs(distance-expected) > .003:
            raise RuntimeError(f'Seat offset is not model-derived: {distance}, expected {expected}')
    else:
        if final['rf_scene_setup_result'] != [2, SLAY, 1, 0] or slay[:3] != [1, 1, ACTOR] or f(slay[3]) > 0 or slay[5]:
            raise RuntimeError(f'Normal Slay/death path failed: {slay}')
        if final['rf_scene_npc_seats'][5:7] != [1, 0] or not live['rf_scene_vehicle_route_state'][3] < route[3] <= DEATH_FRAME+1:
            raise RuntimeError(f'Dead driver did not detach and inhibit retained route: {route}')
    if result['free_pages'] <= 0:
        raise RuntimeError('No free guest memory')
    return dict(result='PASS', actor_uid=ACTOR, host_uid=HOST,
                live_probe_frame=live['frame'], driver_owned_pair_verified=True,
                route_drive_ticks=route[3], driver_death_checked=not link_only,
                limitations='Process-local synthetic route command check; no campaign traversal, save/load, visual animation or full pose orientation claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--link-only', action='store_true', help='Leave driver alive; check completed model-tag offset and continuous ownership')
    args = parser.parse_args()
    if args.prepare_only:
        path, _ = prepare_level(args.prepare_only); print(path); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('npc-jeep-seat-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture, recipe = prepare_level(folder/'level')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag', 'scene-fixture.vpp'}
    original = {n: (DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__, link_only=args.link_only)
    try:
        for n in names: (DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64, b'\0')+b'L12S1.rfl'.ljust(64, b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b''); (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(ROUTE) if args.link_only else U(ROUTE, SLAY))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder, 'seat')
        result = run_guest(folder, 'seat', hdd, FRAMES, 360, snapshot=True,
                           extra_symbols=SYMBOLS, probe=live_probe, probe_frame=15)
        report['native'] = result; report.update(validate(result, recipe, args.link_only))
    except Exception as exc:
        report['error'] = str(exc); raise
    finally:
        for n, data in original.items():
            if data is None: (DISC/n).unlink(missing_ok=True)
            else: (DISC/n).write_bytes(data)
        try: build(folder, 'restore')
        except Exception as exc:
            report['result'] = 'FAIL'; report['restore_build_error'] = str(exc)
            raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None) == data for n, data in original.items())
            if not report['disc_restored']: report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report, indent=2)+'\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']: raise RuntimeError('Test disc restoration failed')


if __name__ == '__main__':
    main()
