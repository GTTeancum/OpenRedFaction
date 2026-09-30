"""Bounded Xbox Follow_Waypoints OFF save/load and cursor-preserving resume.

Authored L12S1 Jeep/miner in CTF06; two fixture nodes, the first at its start.
ON0/OFF60/save90; fresh load probes inactive state, Waypoints60/end72.
No campaign traversal, host input, images or checkpoint byte fabrication.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct
from xemu_npc_jeep_seat import ACTOR, HOST, ROUTE, NODE, SYMBOLS as SEAT_SYMBOLS, prepare_level as prepare_pair, U, F, S, f
from xemu_npc_jeep_detached_save import component
from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from inspect_navigation_records import inspect as inspect_navigation
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

OFF, RESUME, NOOP = 910950, 910951, 910952
SAVE_FRAMES, LOAD_FRAMES = 90, 72
SYMBOLS = dict(SEAT_SYMBOLS, rf_scene_npc_seat_checkpoint=6,
               rf_scene_vehicle_ai_mode=8, rf_scene_vehicle_resume_probe=12,
               rf_scene_world_load_reject=3, rf_scene_npc_checkpoint_reject_state=6,
               rf_scene_vehicle_restore_trace=8, rf_scene_checkpoint_world_reject=9)


def prepare_level(folder):
    path, recipe = prepare_pair(folder)
    raw = read_entry(path, 'L12S1.rfl')
    meta = inspect(io.BytesIO(raw), dict(offset=0, size=len(raw), name='L12S1.rfl'))
    points = [recipe['host_position'], [recipe['host_position'][0], recipe['host_position'][1], recipe['host_position'][2]+100]]
    nav = U(2)
    for index, point in enumerate(points):
        nav += U(NODE+index)+b'\1'+F(2, *point, 1)+U(0)+bytes(4)+F(0)+U(0)
    nav += bytes(2)  # Neighbor lists follow all fixed navigation records.
    nodes = inspect_navigation(nav)
    if len(nodes)!=2 or [r['uid'] for r in nodes]!=[NODE, NODE+1]:
        raise RuntimeError('Two-node fixture failed independent decoding')
    name = b'jeep_seat_route'
    follow = event(ROUTE, 'Follow_Waypoints', 'retained_route', (HOST,), text=name.decode())
    suffix = S(name)+S(b'')+U(1, HOST)+bytes([255]*4)
    if not follow.endswith(suffix): raise RuntimeError('Event helper layout changed')
    follow = follow[:-len(suffix)]+S(name)+S(b'One way')+U(1, HOST)+bytes([255]*4)
    resume_name = 'resume_route'
    resume = bytearray(event(RESUME, 'Set_AI_Mode', resume_name, (HOST,)))
    word_at = 4+2+len('Set_AI_Mode')+12+2+len(resume_name)+1+4+2
    struct.pack_into('<I', resume, word_at, 2)  # Authored Waypoints enum -> action4.
    replacements = {0x20000: nav, 0x10000: U(1)+S(name)+U(2, 0, 1),
                    0x600: U(4)+follow+event(OFF, 'Invert', 'route_off', (ROUTE,))+resume+event(NOOP, 'Invert', 'load_noop')}
    out = bytearray(raw[:meta['sections'][0]['offset']]); offsets = {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = replacements.get(kind, raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind] = len(out); out += U(kind, len(payload))+payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L12S1.rfl'))
    archive(path, [('L12S1.rfl', out)])
    recipe.update(setup_frames={str(ROUTE): 0, str(OFF): 60},
                  load_setup_frames={str(NOOP): 0, str(RESUME): 60}, frames=SAVE_FRAMES,
                  load_frames=LOAD_FRAMES, nodes=points, expected_cursor=1,
                  scope='First node starts under Jeep; only bounded rigid commands, no campaign traversal',
                  staged_fields=['entity transforms', 'two-node navigation/path', 'Follow_Waypoints/Invert/Set_AI_Mode events'])
    (folder/'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    return path, recipe


def live_probe(monitor, mapping):
    result = {name:words(monitor, address(mapping, name), size) for name,size in SYMBOLS.items()}
    result['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return result


def saved_route(payload, recipe):
    block = component(payload, 11)
    if len(block)<40 or block[:4]!=b'RFNS': raise RuntimeError('Missing authored seat wrapper')
    version, size, count = struct.unpack_from('<III', block, 4)
    if (version,count)!=(1,1) or len(block)!=16+size+24: raise RuntimeError('Malformed RFNS row')
    seat = struct.unpack_from('<6I', block, 16+size)
    if seat!=(ACTOR, HOST, recipe['seat']['index'], 1, 1, 0): raise RuntimeError(f'Live driver ownership lost: {seat}')
    vehicle = block[16:16+size]
    if len(vehicle)!=200 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<II',vehicle,4)!=(3,160) or struct.unpack_from('<II',vehicle,16)!=(3,1):
        raise RuntimeError('Missing alive unoccupied Jeep with route trailer')
    if vehicle[160:164]!=b'RFVR': raise RuntimeError('Inactive route was omitted')
    values = struct.unpack_from('<9I', vehicle, 164)
    if values!=(3, ROUTE, 1, 0, 0, 2, 0, 0, 1):
        raise RuntimeError(f'RFVR3 failed to preserve inactive nonzero cursor: {values}')
    return dict(version=3,event=ROUTE,index=1,reverse=0,mode=0,count=2,ai_action=0,active=0,kind=1,
                vehicle_position=list(struct.unpack_from('<3f',vehicle,32)),seat_active=1)


def check_owner(x,live=False):
    seats = x['rf_scene_npc_seats']; pose = x['rf_scene_npc_seat_probe']; vehicle = x['rf_scene_vehicle_state']
    expected=[0,1] if live else [1,0]
    if seats[:4]!=[1,1,0,0] or seats[5:7]!=expected or seats[9] or pose[:2]!=[ACTOR,HOST] or pose[6]!=13 or pose[7]!=pose[3]:
        raise RuntimeError(f'Authored driver no longer attached: {seats}, {pose}')
    if pose[2]==pose[3] or 0xffffffff in pose[2:4] or f(pose[4])<=0 or vehicle[3] or vehicle[5] or vehicle[12]!=pose[3]:
        raise RuntimeError('Invalid live driver/Jeep owner or player possession')
    if x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7] or any(x['rf_scene_script_slays']) or x['rf_scene_vehicle_route_state'][7]:
        raise RuntimeError('Unexpected death/fire/route error')


def check_run(result, frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    check_owner(result['extra'])


def validate_source(saved, payload, recipe):
    check_run(saved, SAVE_FRAMES); x=saved['extra']; state=saved['checkpoint_state']
    row=saved_route(payload, recipe); route=x['rf_scene_vehicle_route_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload): raise RuntimeError(f'Ordinary source save failed: {state}')
    if route[:3]!=[0,1,2] or not 1<=route[3]<=61 or route[4]!=1 or route[5]!=ROUTE:
        raise RuntimeError(f'ON/OFF did not retain the reached first cursor: {route}')
    if x['rf_scene_setup_result']!=[2,OFF,3,0] or any(x['rf_scene_vehicle_resume_probe']):
        raise RuntimeError('OFF event failed or source unexpectedly resumed')
    return row


def validate(saved, loaded, payload, recipe):
    row=validate_source(saved,payload,recipe); check_run(loaded,LOAD_FRAMES)
    x=loaded['extra']; before=loaded['probe']; check_owner(before,live=True)
    state=loaded['checkpoint_state']; route=x['rf_scene_vehicle_route_state']; stopped=before['rf_scene_vehicle_route_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary route restore failed: {state}')
    if not 10<=before['frame']<60 or stopped[:5]!=[0,1,2,0,0] or stopped[5]!=ROUTE:
        raise RuntimeError(f'Saved inactive cursor was not observed before resume: {before["frame"]}, {stopped}')
    if any(before['rf_scene_vehicle_resume_probe']) or before['rf_scene_vehicle_ai_mode'][0]:
        raise RuntimeError('Waypoints activated before the post-load event')
    if x['rf_scene_npc_seat_checkpoint'][:4]!=[0,1,1,1] or x['rf_scene_npc_seat_checkpoint'][5]:
        raise RuntimeError('Live authored seat did not restore once')
    if route[:3]!=[1,1,2] or not route[3] or route[4] or route[5]!=ROUTE:
        raise RuntimeError(f'Resume reset cursor/replayed arrivals/failed to drive: {route}')
    mode=x['rf_scene_vehicle_ai_mode']; p=x['rf_scene_vehicle_resume_probe']
    if mode[0]!=1 or mode[1:4]!=[4,1,0] or mode[4] or mode[6]!=4:
        raise RuntimeError(f'Expected one ordinary Waypoints mode change after load: {mode}')
    if p[:5]!=[1,1,1,ROUTE,ROUTE] or p[5:8]!=p[8:11] or p[11]!=1 or not all(math.isfinite(f(v)) for v in p[5:11]):
        raise RuntimeError(f'Mode change lost cursor/event or teleported host: {p}')
    if x['rf_scene_setup_result']!=[2,RESUME,34,0]: raise RuntimeError('Unexpected load setup event replay')
    return dict(result='PASS',saved=row,pre_resume_frame=before['frame'],pre_resume_route=stopped,
                resumed_route=route,resume_transition=p,
                free_pages=min(saved['free_pages'],loaded['free_pages']),
                limitations='Two-node isolated fixture; nonzero cursor comes from node under initial host, not campaign travel. No complete route, player gunner or visual claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--validate-existing',type=Path)
    args=parser.parse_args()
    if args.prepare_only: print(prepare_level(args.prepare_only)[0]); return
    if args.validate_existing:
        folder=args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'): raise RuntimeError('Disc restoration unconfirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()),json.loads((folder/'load/result.json').read_text()),(folder/'save/xbox-world.rfwc').read_bytes(),json.loads((folder/'level/recipe.json').read_text())),indent=2)); return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-route-off-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',scope=__doc__,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(ROUTE,OFF));(DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved']=validate_source(saved,payload,recipe)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'campaign-setup.bin').write_bytes(U(NOOP,RESUME))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(LOAD_FRAMES*48))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=20,allow_guest_error=True)
        report['phases']['load']=loaded;report.update(validate(saved,loaded,payload,recipe))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_build_error']=str(exc);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Fixture disc restoration failed')


if __name__=='__main__':main()
