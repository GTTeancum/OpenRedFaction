"""Bounded Xbox metadata-selected Jeep in CTF06 without level-name override.

Copies the proven grounded original Jeep7629/miner7646 fixture byte-for-byte
under ctf06.rfl instead of L12S1.rfl. Ordinary Use90 boards, Follow_Waypoints120
moves the real chassis. No vehicle-test/profile override, host input or images.
"""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U
from check_ai_projectile_ordinary import archive
from xemu_npc_jeep_gunner import prepare_level as prepare_grounded,check_driver,SYMBOLS as GUNNER_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST,ROUTE,f
from xemu_npc_jeep_detached_save import STOP
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=180
SYMBOLS=dict(GUNNER_SYMBOLS,rf_scene_vehicle_selection=12,rf_scene_vehicle_enabled=1)


def replay():
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,int(frame==90),0,0,0,0)
        for frame in range(FRAMES))


def prepare_level(folder):
    fixture,recipe=prepare_grounded(folder)
    raw=read_entry(fixture,'L12S1.rfl')
    archive(fixture,[('ctf06.rfl',raw)])
    if read_entry(fixture,'ctf06.rfl')!=raw:raise RuntimeError('Fixture rename altered level bytes')
    recipe.update(scope=__doc__,entry_alias='ctf06.rfl',frames=FRAMES,
                  rfl_sha256=hashlib.sha256(raw).hexdigest(),
                  replay={'board':90,'route_on':120,'probe':150,'end':FRAMES},
                  expected_selection={'uid':HOST,'profile':3,'source':2,'legacy':False},
                  limitations='One eligible Jeep only; competing candidates/rejection ordering, other profiles, level transitions and audiovisual behavior remain unverified.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return fixture,recipe


def live_probe(monitor,mapping):
    result={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    result['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return result


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    live=result['probe'];end=result['extra']
    if not 150<=live['frame']<FRAMES:raise RuntimeError('Missing moving/occupied live probe')
    npc,host=check_driver(live)
    if check_driver(end,teardown=True)!=(npc,host):raise RuntimeError('Driver/host identity changed')
    selection=live['rf_scene_vehicle_selection']
    if selection!=end['rf_scene_vehicle_selection'] or selection!=[2,1,0,0,0,0,0,1,HOST,3,2,0]:
        raise RuntimeError(f'Metadata fallback did not uniquely select real Jeep7629: {selection}')
    for values in (live,end):
        if values['rf_scene_vehicle_enabled']!=[3] or values['rf_scene_vehicle_state'][1:4]!=[1,0,1]:
            raise RuntimeError('Selected profile did not produce an ordinary occupied Jeep')
        g=values['rf_scene_jeep_npc_gunner'];player=g[9]
        if player in (npc,host,0xffffffff) or g[:3]!=[1,0,0] or g[4:9]!=[1,host,npc,npc,player] or g[10:16]!=[host,1,1,0,0,1]:
            raise RuntimeError(f'Ordinary Use failed player gunner/NPC driver ownership: {g}')
        route=values['rf_scene_vehicle_route_state']
        if route[0]!=1 or route[3]<10 or route[5:7]!=[ROUTE,host] or values['rf_scene_apc_primary'][1]:
            raise RuntimeError('Authored route did not drive selected chassis or unexpected firing occurred')
    if end['rf_scene_setup_result']!=[2,ROUTE,28,0]:raise RuntimeError('Ordinary park/route setup failed')
    initial=recipe['host_position'];final=[f(v) for v in end['rf_scene_vehicle_state'][6:9]]
    horizontal=math.hypot(final[0]-initial[0],final[2]-initial[2])
    if not all(math.isfinite(v) for v in final) or horizontal<.1:
        raise RuntimeError(f'Selected vehicle did not physically move: {initial}, {final}')
    if end['rf_scene_vehicle_route_state'][3]<=live['rf_scene_vehicle_route_state'][3]:
        raise RuntimeError('Route stopped while player remained mounted')
    return dict(result='PASS',selection=selection,player_gunner_with_living_driver=True,
                actual_horizontal_displacement=horizontal,no_level_profile_override=True,
                free_pages=result['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-selection-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,ROUTE));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'selection')
        result=run_guest(folder,'selection',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
            probe=live_probe,probe_frame=150,allow_guest_error=True)
        report['native']=result;report.update(validate(result,recipe))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_error']=str(exc);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
