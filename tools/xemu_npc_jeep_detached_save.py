"""Bounded Xbox detached authored Jeep-driver corpse ordinary save/fresh-load.

Reuses L12S1 miner7646/Jeep7629 in CTF06. Set_AI_Mode parks the host at frame0;
Slay_Object kills its driver at frame60. Source240/load12 neutral frames, no
route activation, host input, images or fabricated checkpoint bytes.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from xemu_npc_jeep_seat import ACTOR, HOST, SLAY, SYMBOLS as SEAT_SYMBOLS, prepare_level as prepare_pair, U, f
from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_corpse_save import dead_record
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

STOP, SAVE_FRAMES, LOAD_FRAMES = 910943, 240, 12
SYMBOLS = dict(SEAT_SYMBOLS, rf_scene_npc_jeep_detached_probe=16,
               rf_scene_npc_seat_checkpoint=6, rf_scene_vehicle_ai_mode=8,
               rf_scene_live_corpses=8, rf_scene_corpse_checkpoint=8,
               rf_scene_live_death_audio=4, rf_scene_world_load_reject=3,
               rf_scene_npc_checkpoint_reject_state=6)


class FixtureLimitation(RuntimeError):
    """Real death/rigid motion did not produce the narrowly admitted contact."""


def prepare_level(folder):
    path, recipe = prepare_pair(folder)
    original = read_entry(path, 'L12S1.rfl')
    meta = inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='L12S1.rfl'))
    # The ordinary authored enum0 maps to action1 (catatonic). No route is
    # started; retain only the existing Slay and this host-mode event.
    events = U(2)+event(STOP, 'Set_AI_Mode', 'park_before_driver_death', (HOST,))+event(SLAY, 'Slay_Object', 'seat_driver_death', (ACTOR,))
    out = bytearray(original[:meta['sections'][0]['offset']]); offsets = {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = events if kind == 0x600 else original[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind] = len(out); out += U(kind, len(payload))+payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L12S1.rfl'))
    archive(path, [('L12S1.rfl', out)])
    recipe.update(setup_frames={str(STOP): 0, str(SLAY): 60}, frames=SAVE_FRAMES,
                  route_activated=False, load_frames=LOAD_FRAMES,
                  staged_fields=['authored entity transforms', 'Set_AI_Mode and Slay events; retained unused navigation'])
    (folder/'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    return path, recipe


def component(payload, kind):
    if len(payload)<320 or payload[:4]!=b'RFWC':
        raise RuntimeError('Missing ordinary world envelope')
    actual, offset, size = struct.unpack_from('<III', payload, 128+(kind-1)*12)
    if actual!=kind or offset<320 or offset+size>len(payload):
        raise RuntimeError(f'Invalid component {kind}')
    return payload[offset:offset+size]


def saved_rows(payload, recipe):
    block = component(payload, 11)
    if len(block)<40 or block[:4]!=b'RFNS':
        raise RuntimeError('Expected direct RFNS wrapper for the Jeep fixture')
    version, inner, count = struct.unpack_from('<III', block, 4)
    if (version, count)!=(1, 1) or len(block)!=16+inner+24:
        raise RuntimeError('Invalid single authored seat envelope')
    seat = struct.unpack_from('<6I', block, 16+inner)
    if seat!=(ACTOR, HOST, recipe['seat']['index'], 1, 0, 0):
        raise RuntimeError(f'RFNS did not preserve detached authored driver: {seat}')
    vehicle = block[16:16+inner]
    if len(vehicle)!=200 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<4I', vehicle, 4)[:2]!=(3, 160):
        raise RuntimeError('Expected RFVC3 Jeep and RFVR3 mode trailer')
    if struct.unpack_from('<II', vehicle, 16)!=(3, 1):
        raise RuntimeError('Saved Jeep is not alive/unoccupied')
    if vehicle[160:164]!=b'RFVR' or struct.unpack_from('<9I', vehicle, 164)!=(3, 0, 0, 0, 0, 0, 1, 0, 0):
        raise RuntimeError('Saved Jeep lost parked AI mode or gained a route')
    position = struct.unpack_from('<3f', vehicle, 32)
    basis = struct.unpack_from('<9f', vehicle, 44)
    blob = component(payload, 2)
    if len(blob)<664 or blob[:4]!=b'RFNC' or struct.unpack_from('<I', blob, 4)[0]!=10 or struct.unpack_from('<I', blob, 16)[0]!=1:
        raise RuntimeError('Expected one RFNC10 authored miner')
    row = blob[64:]
    uid, _, retired, flags = struct.unpack_from('<4I', row)
    health, armor = struct.unpack_from('<2f', row, 20)
    actor_position = struct.unpack_from('<3f', row, 28)
    ai_mode = struct.unpack_from('<i', row, 524)[0]
    support = struct.unpack_from('<I', row, 572)[0]
    if uid!=ACTOR or retired or flags&0x4000 or not math.isfinite(health) or health>0 or ai_mode==13 or support:
        raise RuntimeError('Saved former driver is not a detached terminal corpse')
    death = dead_record(payload, ACTOR)
    expected = [position[k]+sum(recipe['seat']['position'][j]*basis[j*3+k] for j in range(3)) for k in range(3)]
    delta = max(abs(a-b) for a, b in zip(actor_position, expected))
    if not all(math.isfinite(v) for v in position+basis+actor_position) or math.dist(actor_position,expected)>.25:
        raise FixtureLimitation(f'Corpse outside former Jeep seat envelope: maximum delta={delta:.9g}; actor={actor_position}, tag={expected}. Keep normal physics; schedule Slay later after parked Jeep settles via another ordinary Set_AI_Mode setup event. Never rewrite saved bytes.')
    return dict(actor_uid=uid, host_uid=HOST, seat_tag=seat[2], seat_active=0,
                health=health, armor=armor, ai_mode=ai_mode, position=list(actor_position),
                jeep_position=list(position), tag_position=expected, tag_delta=delta,
                route_active=0, vehicle_ai_mode=1, corpse=death)


def check_run(result, frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    x = result['extra']; p = x['rf_scene_npc_jeep_detached_probe']
    if p[:2]!=[ACTOR, HOST] or p[2]==p[3] or 0xffffffff in p[2:4] or p[15]!=1:
        raise RuntimeError(f'Missing actual detached actor/host probe: {p}')
    if not math.isfinite(f(p[4])) or f(p[4])>0 or p[5]==13 or p[6:9]!=[0xffffffff]*3 or p[9:12]!=[0, 0, 1]:
        raise RuntimeError(f'Dead actor acquired seat/driver ownership or resumed route: {p}')
    if not all(math.isfinite(f(v)) for v in p[12:15]):
        raise RuntimeError('Nonfinite detached actor pose')
    seat = x['rf_scene_npc_seats']; route = x['rf_scene_vehicle_route_state']
    if seat[:4]!=[1, 1, 0, 0] or seat[6] or seat[9] or route[0] or route[3] or route[7]:
        raise RuntimeError(f'Seat lifecycle/parked route failed: {seat}, {route}')
    if x['rf_scene_vehicle_state'][3] or x['rf_scene_vehicle_state'][5] or x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7]:
        raise RuntimeError('Unexpected possession, handheld fire or gameplay error')
    corpses = x['rf_scene_live_corpses']
    if corpses[:2]!=[1, 1] or any(corpses[5:]):
        raise RuntimeError(f'Expected one owned dead miner body: {corpses}')
    return p


def validate_source(saved, payload, recipe):
    p = check_run(saved, SAVE_FRAMES); x = saved['extra']; state = saved['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload) or x['rf_scene_npc_seat_checkpoint'][0]!=1 or x['rf_scene_npc_seat_checkpoint'][5]:
        raise RuntimeError(f'Ordinary detached source save failed: {state}')
    if x['rf_scene_setup_result']!=[2, SLAY, 1, 0] or x['rf_scene_script_slays'][:3]!=[1, 1, ACTOR] or x['rf_scene_npc_seats'][5]!=1:
        raise RuntimeError('Expected ordinary single Slay and seat detach')
    row = saved_rows(payload, recipe)
    if f(p[4])!=row['health'] or max(abs(f(p[12+i])-row['position'][i]) for i in range(3))>.001:
        raise RuntimeError('Serialized corpse differs from live detached owner')
    return row


def validate(saved, loaded, payload, recipe):
    row = validate_source(saved, payload, recipe); p = check_run(loaded, LOAD_FRAMES)
    x = loaded['extra']; state = loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary detached world restore failed: {state}')
    if x['rf_scene_npc_seat_checkpoint'][:4]!=[0, 1, 1, 0] or x['rf_scene_npc_seat_checkpoint'][5]:
        raise RuntimeError('RFNS did not restore exactly one inactive seat')
    if x['rf_scene_corpse_checkpoint']!=[1, 1, 1, 0, ACTOR, 0, 1, 1]:
        raise RuntimeError('Saved dead actor/corpse owner did not restore once')
    if any(x['rf_scene_script_slays']) or any(x['rf_scene_setup_result']) or any(x['rf_scene_live_death_audio']):
        raise RuntimeError('Loading replayed Slay/setup/death effects')
    if f(p[4])!=row['health'] or max(abs(f(p[12+i])-row['position'][i]) for i in range(3))>.001:
        raise RuntimeError('Loaded corpse health/position differs from saved detached actor')
    return dict(result='PASS', saved=row, restored_probe=p,
                free_pages=min(saved['free_pages'], loaded['free_pages']),
                limitations='Parked Jeep and settled former driver only; no tilted/falling corpse, moving route, player gunner or campaign traversal claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--validate-existing', type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        print(prepare_level(args.prepare_only)[0]); return
    if args.validate_existing:
        folder = args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):
            raise RuntimeError('Disc restoration not confirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()), json.loads((folder/'load/result.json').read_text()), (folder/'save/xbox-world.rfwc').read_bytes(), json.loads((folder/'level/recipe.json').read_text())), indent=2)); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('npc-jeep-detached-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture, recipe = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag', 'scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__, phases={})
    try:
        for n in names: (DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b''); (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP, SLAY))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder, 'save')
        saved = run_guest(folder, 'save', hdd, SAVE_FRAMES, 480, capture_world=True, extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['save'] = saved
        payload = (folder/'save/xbox-world.rfwc').read_bytes()
        report['saved'] = validate_source(saved, payload, recipe)
        (DISC/'campaign-setup.bin').unlink(); (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(LOAD_FRAMES*48))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, LOAD_FRAMES, 360, extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['load'] = loaded; report.update(validate(saved, loaded, payload, recipe))
    except Exception as exc:
        report['error'] = str(exc)
        if isinstance(exc, FixtureLimitation): report['fixture_limitation'] = str(exc)
        raise
    finally:
        for n, data in original.items():
            if data is None: (DISC/n).unlink(missing_ok=True)
            else: (DISC/n).write_bytes(data)
        try: build(folder, 'restore')
        except Exception as exc:
            report['result']='FAIL'; report['restore_build_error']=str(exc); raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']: report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']: raise RuntimeError('Fixture disc restoration failed')


if __name__=='__main__': main()
