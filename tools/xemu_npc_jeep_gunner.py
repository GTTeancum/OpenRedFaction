"""Bounded Xbox authored NPC Jeep driver plus player gunner coexistence.

Retains the L12S1 Jeep/miner records in the CTF06 testbed. Normal process-local
Use boards at90, route starts120, cycle140 must not steal the driver seat, fire
145..169, Use210 exits. 240frames; no campaign traversal, images or host input.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from xemu_npc_jeep_seat import ACTOR, HOST, ROUTE, SYMBOLS as SEAT_SYMBOLS, prepare_level as prepare_pair, U, S, f
from xemu_npc_jeep_detached_save import STOP
from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES = 240
SYMBOLS = dict(SEAT_SYMBOLS, rf_scene_jeep_npc_gunner=16,
               rf_scene_jeep_seats=8, rf_scene_apc_primary=8,
               rf_scene_vehicle_ai_mode=8, rf_scene_jeep_entry_probe=12)


def prepare_level(folder):
    path, recipe = prepare_pair(folder)
    raw = read_entry(path, 'L12S1.rfl')
    meta = inspect(io.BytesIO(raw), dict(offset=0, size=len(raw), name='L12S1.rfl'))
    pair = entity_rows(raw)
    if [r['uid'] for r in pair] != [HOST, ACTOR]:
        raise RuntimeError('Unexpected authored seat fixture records')
    records = []
    # Existing fixture is6m from player; the installed use radius is5m. Move
    # only both entity transforms4m toward the same unchanged player spawn.
    # The native entry still has to pass the real head/world admission query.
    for row in pair:
        value = bytearray(row['raw']); at = row['transform']+8
        z, = struct.unpack_from('<f', value, at)
        struct.pack_into('<f', value, at, z-4)
        if value[:at] != row['raw'][:at] or value[at+4:] != row['raw'][at+4:]:
            raise RuntimeError('Fixture changed a non-position entity field')
        records.append(value)
    name = b'jeep_seat_route'
    follow = event(ROUTE, 'Follow_Waypoints', 'gunner_route', (HOST,), text=name.decode())
    suffix = S(name)+S(b'')+U(1, HOST)+bytes([255]*4)
    if not follow.endswith(suffix): raise RuntimeError('Follow_Waypoints layout changed')
    follow = follow[:-len(suffix)]+S(name)+S(b'One way')+U(1, HOST)+bytes([255]*4)
    # Ordinary event delay: setup fires at60, route executes at120.
    follow = bytearray(follow)
    delay_offset = 4+2+len('Follow_Waypoints')+12+2+len('gunner_route')+1
    struct.pack_into('<f', follow, delay_offset, 1.0)
    events = U(2)+event(STOP, 'Set_AI_Mode', 'park_for_gunner', (HOST,))+follow
    replacements = {0x30000: U(2)+b''.join(records), 0x600: events}
    out = bytearray(raw[:meta['sections'][0]['offset']]); offsets = {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = replacements.get(kind, raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind] = len(out); out += U(kind, len(payload))+payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L12S1.rfl'))
    checked = entity_rows(out)
    if [r['raw'] for r in checked] != [bytes(r) for r in records]:
        raise RuntimeError('Independent entity round trip failed')
    archive(path, [('L12S1.rfl', out)])
    for key in ('host_position', 'actor_initial_position'):
        recipe[key] = list(recipe[key]); recipe[key][2] -= 4
    recipe.update(frames=FRAMES, setup_frames={str(STOP): 0, str(ROUTE): 120},
                  staged_fields=['authored entity transforms', 'bounded single-node route and Set_AI_Mode events'],
                  replay={'board':90, 'route_on':120, 'occupied_driver_switch':140,
                          'fire':[145,169], 'exit':210, 'move_backward_while_gunner':[145,169]},
                  scope='NPC driver/player gunner control and safe exit; no save, campaign route or visual claim')
    (folder/'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path, recipe


def replay():
    rows = []
    for frame in range(FRAMES):
        firing = 145 <= frame < 170
        # RFI6 move3,look2,crouch,jump,use,fire,reload,cycle,alternate.
        rows.append(struct.pack('<5f7I', 0,0,-float(firing),0,0,
                                0,0,int(frame in (90,210)),int(firing),0,int(frame==140),0))
    return b'RFI6'+U(48)+b''.join(rows)


def live_probe(monitor, mapping):
    values = {name:words(monitor, address(mapping, name), size) for name,size in SYMBOLS.items()}
    values['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return values


def check_driver(values, *, teardown=False):
    seats = values['rf_scene_npc_seats']; pair = values['rf_scene_npc_seat_probe']
    # Completion follows normal level teardown: its one authored binding is
    # detached, while the probe/vehicle/gunner arrays retain the last live pose.
    # Live probes MUST still show attached1/detached0; never accept teardown as
    # evidence that a driver existed during the player-controlled portion.
    counts = [1,0] if teardown else [0,1]
    if seats[:4] != [1,1,0,0] or seats[4] < 2 or seats[5:9] != counts+[ACTOR,HOST] or seats[9]:
        raise RuntimeError(f'Authored NPC driver seat changed: {seats}')
    if pair[:2] != [ACTOR,HOST] or pair[2] == pair[3] or 0xffffffff in pair[2:4] or pair[6] != 13 or pair[7] != pair[3] or f(pair[4]) != 1.0:
        raise RuntimeError(f'NPC driver identity/action/health lost: {pair}')
    if values['rf_scene_vehicle_state'][12] != pair[3] or not all(math.isfinite(f(v)) for v in pair[4:6]+pair[8:11]):
        raise RuntimeError('Driver points to a different registered Jeep or has invalid live pose/vitals')
    if any(values['rf_scene_script_slays']) or values['rf_scene_enemy_combat'][2] or values['rf_scene_enemy_combat'][7]:
        raise RuntimeError('Unexpected NPC Slay, handheld fire or combat error')
    if values['rf_scene_vehicle_state'][5] or values['rf_scene_vehicle_route_state'][7]:
        raise RuntimeError('Vehicle/route runtime error')
    return pair[2], pair[3]


def validate(result):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or result['memory_bytes'] != 64*1024*1024 or result['free_pages'] <= 0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live = result['probe']; final = result['extra']
    if not 180 <= live['frame'] < 210:
        raise RuntimeError('Missed boarded/fired/pre-exit window; no inferred coexistence pass')
    npc, host = check_driver(live)
    if check_driver(final, teardown=True) != (npc,host): raise RuntimeError('Driver/host registry identity changed')
    g = live['rf_scene_jeep_npc_gunner']; end = final['rf_scene_jeep_npc_gunner']
    player = g[9]
    if player in (npc,host,0xffffffff) or g[:3] != [1,0,1] or g[3] < 2 or g[4:9] != [1,host,npc,npc,player] or g[10:16] != [host,1,1,0,0,1]:
        raise RuntimeError(f'Gunner stole driver seat, failed control or switched seat: {g}')
    if live['rf_scene_vehicle_state'][1:4] != [1,0,1] or live['rf_scene_jeep_seats'][1] or live['rf_scene_jeep_seats'][4] != 1:
        raise RuntimeError('Player session is not exclusively gunner')
    route = live['rf_scene_vehicle_route_state']
    if route[0] != 1 or route[3] < 10 or route[5:7] != [ROUTE,host]:
        raise RuntimeError(f'Authored NPC route stopped while player was aboard: {route}')
    shots = live['rf_scene_apc_primary'][1]
    if shots < 1:
        raise RuntimeError('Gunner fire did not launch a real Jeep Gun round')
    # The shared clipless scheduler stops immediately when held/allowed is0.
    # Existing rounds may keep travelling, but cannot become new launches.
    if final['rf_scene_apc_primary'][1] != shots or final['rf_scene_apc_primary'][7] != live['rf_scene_apc_primary'][7]:
        raise RuntimeError('Jeep kept launching/spending ammunition after released gunner fire')
    if end[:3] != [1,1,1] or end[4:9] != [0,host,npc,npc,0xffffffff] or end[9:16] != [player,0xffffffff,0,1,0,0,1]:
        raise RuntimeError(f'Use exit failed to preserve NPC driver and release player: {end}')
    if final['rf_scene_vehicle_state'][1:4] != [1,1,0]:
        raise RuntimeError('Ordinary safe-exit publication did not complete')
    if final['rf_scene_vehicle_route_state'][3] <= route[3]:
        raise RuntimeError('NPC route did not continue after player exit')
    if final['rf_scene_setup_result'] != [2,ROUTE,28,0]:
        raise RuntimeError('Expected normal park/route authored event setup did not complete')
    return dict(result='PASS', live_probe_frame=live['frame'], actor_uid=ACTOR, host_uid=HOST,
                player_gunner_separate_from_npc_driver=True,
                mounted_gun_launches=final['rf_scene_apc_primary'][1],
                occupied_driver_switch_rejected=True, safe_exit_preserved_npc_driver=True,
                post_release_new_launches=0,
                limitations='Functional ownership/route-command/launch/exit fixture only; no projectile-source runtime, actual travel-distance, ordinary coexistence save, campaign route, visual or destruction-ejection claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        path, _ = prepare_level(args.prepare_only); print(path); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('npc-jeep-gunner-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture, recipe = prepare_level(folder/'level')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe)
    try:
        for n in names: (DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b''); (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,ROUTE))
        (DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'gunner')
        result = run_guest(folder,'gunner',hdd,FRAMES,420,snapshot=True,
                           extra_symbols=SYMBOLS,probe=live_probe,probe_frame=180)
        report['native'] = result; report.update(validate(result))
    except Exception as exc:
        report['error'] = str(exc); raise
    finally:
        for n,data in original.items():
            if data is None: (DISC/n).unlink(missing_ok=True)
            else: (DISC/n).write_bytes(data)
        try: build(folder,'restore')
        except Exception as exc:
            report['result']='FAIL'; report['restore_build_error']=str(exc); raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']: report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']: raise RuntimeError('Test disc restoration failed')


if __name__ == '__main__':
    main()
