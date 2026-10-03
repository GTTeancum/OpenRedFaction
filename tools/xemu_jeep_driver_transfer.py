"""Bounded Xbox gunner-to-empty-driver transfer after real NPC driver death.

Grounded authored Jeep/miner pair; STOP0, board90, occupied cycle100 (rejected),
ordinary delayed Slay120, empty-driver cycle150, throttle160..190, probe200,
finish240. Optional --exit requests normal Use220. No fake ownership, forced
drive state, scripted route, host input, images or campaign traversal.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from xemu_npc_jeep_gunner import prepare_level as prepare_gunner,SYMBOLS as BASE_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST,SLAY,U,f
from xemu_npc_jeep_detached_save import STOP
from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=240
SYMBOLS=dict(BASE_SYMBOLS,rf_scene_jeep_driver_transfer=9)


def replay(exit_requested=False):
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,float(160<=frame<=190),0,0,
        0,0,int(frame==90 or (exit_requested and frame==220)),0,0,int(frame in (100,150)),0)
        for frame in range(FRAMES))


def prepare_level(folder,exit_requested=False):
    path,recipe=prepare_gunner(folder)
    raw=read_entry(path,'L12S1.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L12S1.rfl'))
    before=[r['raw'] for r in entity_rows(raw)]
    kind,name='Slay_Object','release_jeep_driver'
    slay=bytearray(event(SLAY,kind,name,(ACTOR,)))
    struct.pack_into('<f',slay,4+2+len(kind)+12+2+len(name)+1,1.0)
    events=U(2)+event(STOP,'Set_AI_Mode','park_for_transfer',(HOST,))+slay
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        k=int(section['type'],16)
        payload=events if k==0x600 else raw[section['offset']+8:section['offset']+8+section['size']]
        offsets[k]=len(out);out+=U(k,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L12S1.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=before:raise RuntimeError('Transfer fixture changed grounded authored entity records')
    archive(path,[('L12S1.rfl',out)])
    recipe.pop('route',None)
    recipe.update(scope=__doc__,setup_frames={str(STOP):0,str(SLAY):120},
        route_active=False,replay=dict(board=90,occupied_cycle=100,driver_slay=120,
        transfer_cycle=150,throttle=[160,190],probe=200,exit=220 if exit_requested else None),
        assumptions='Parked speed and real head clearance must pass in native telemetry; not bypassed.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay(exit_requested))
    return path,recipe


def probe(monitor,mapping):
    x={n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return x


def validate(result,exit_requested=False):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0 or result['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB transfer run')
    live=result['probe'];end=result['extra']
    if not 200<=live['frame']<220:raise RuntimeError('Missed active player-driver probe')
    g=live['rf_scene_jeep_npc_gunner'];host,player=g[5],g[9]
    if host==player or 0xffffffff in (host,player):raise RuntimeError('Missing distinct player/host handles')
    transfer=live['rf_scene_jeep_driver_transfer']
    if transfer[:6]!=[1,0,0,1,host,player] or end['rf_scene_jeep_driver_transfer']!=transfer:
        raise RuntimeError(f'Transfer failed/repeated/moved/blocked: {transfer}')
    if g[:3]!=[1,0,1] or g[4:9]!=[0,host,player,player,0xffffffff] or g[10:]!=[host,0,0,1,0,1]:
        raise RuntimeError(f'Occupied attempt was not rejected or player did not become actual driver: {g}')
    if live['rf_scene_vehicle_state'][1:4]!=[1,0,1] or live['rf_scene_jeep_seats'][3]!=1 or not live['rf_scene_jeep_seats'][1]:
        raise RuntimeError('Transfer reboarded or failed ordinary seat-change publication')
    for x in (live,end):
        seat=x['rf_scene_npc_seats'];slay=x['rf_scene_script_slays']
        if seat[:4]!=[1,1,0,0] or seat[5:9]!=[1,0,ACTOR,HOST] or seat[9]:
            raise RuntimeError(f'Actual dead driver did not detach once: {seat}')
        if x['rf_scene_setup_result']!=[2,SLAY,1,0] or slay[:3]!=[1,1,ACTOR] or f(slay[3])>0 or slay[5]:
            raise RuntimeError(f'Ordinary driver Slay failed: {slay}')
        if x['rf_scene_vehicle_state'][5] or x['rf_scene_vehicle_route_state'][7] or x['rf_scene_enemy_combat'][7] or x['rf_scene_enemy_combat'][2]:
            raise RuntimeError('Vehicle/route/NPC error or unexpected handheld fire')
        if x['rf_scene_vehicle_route_state'][0] or x['rf_scene_vehicle_route_state'][3] or x['rf_scene_apc_primary'][1]:
            raise RuntimeError('Authored route or mounted fire contaminated driving check')
    start=[f(v) for v in transfer[6:9]];finish=[f(v) for v in live['rf_scene_vehicle_state'][6:9]]
    if not all(math.isfinite(v) for v in start+finish):raise RuntimeError('Nonfinite host pose')
    distance=math.sqrt(sum((finish[i]-start[i])**2 for i in (0,2)))
    if distance<=.1:raise RuntimeError(f'Real throttle did not move Jeep horizontally: {start} -> {finish}')
    final=end['rf_scene_jeep_npc_gunner']
    if final[0]!=1 or final[2]!=1:raise RuntimeError('Unexpected extra board/switch request')
    if exit_requested:
        if end['rf_scene_vehicle_state'][1:4]!=[1,1,0] or final[6:9]!=[0xffffffff]*3 or final[10]!=0xffffffff:
            raise RuntimeError('Optional ordinary driver exit did not release ownership safely')
    elif end['rf_scene_vehicle_state'][1:4]!=[1,0,1] or final[6:9]!=[player,player,0xffffffff] or final[10:]!=[host,0,0,1,0,1]:
        raise RuntimeError('Player driver ownership did not persist through endpoint')
    return dict(result='PASS',player=player,host=host,transfer=transfer,
                moved_horizontal=distance,exit_checked=exit_requested,
                limitations='One real parked seat transfer and throttle response; no visual/audio, save/load, moving-seat or campaign claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--exit',action='store_true',dest='exit_requested');args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only,args.exit_requested)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('jeep-driver-transfer-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level',args.exit_requested)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,SLAY));(DISC/'player-replay.bin').write_bytes(replay(args.exit_requested))
        build(folder,'transfer');report['native']=run_guest(folder,'transfer',hdd,FRAMES,420,snapshot=True,
            extra_symbols=SYMBOLS,allow_guest_error=True,probe=probe,probe_frame=200)
        report.update(validate(report['native'],args.exit_requested))
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
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
