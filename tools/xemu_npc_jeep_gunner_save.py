"""Bounded Xbox NPC-driver/player-gunner ordinary save and fresh continuation.

Source180: grounded Use90, independent aim100..104, route120, quiet save180. Freshload90:
no setup events, real fire20..34, pre-exit probe40, safe Use60 exit. No images,
host input, campaign route, velocity injection or fabricated checkpoint bytes.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_npc_jeep_gunner import prepare_level as prepare_gunner, check_driver, SYMBOLS as GUNNER_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST,ROUTE,U,f
from xemu_npc_jeep_detached_save import STOP,component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES=180
LOAD_FRAMES=90
SYMBOLS=dict(GUNNER_SYMBOLS,rf_scene_jeep_gunner_restore=38,
             rf_scene_npc_seat_checkpoint=6,rf_scene_world_load_reject=3,
             rf_scene_npc_checkpoint_reject_state=6,rf_scene_vehicle_restore_trace=8,
             rf_scene_checkpoint_world_reject=9,rf_scene_world_player_probe=3)


def replay(load=False):
    rows=[]
    for frame in range(LOAD_FRAMES if load else SAVE_FRAMES):
        aim=not load and 100<=frame<105
        rows.append(struct.pack('<5f7I',0,0,0,.15 if aim else 0,.3 if aim else 0,
                                0,0,int(frame==(60 if load else 90)),int(load and 20<=frame<35),0,0,0))
    return b'RFI6'+U(48)+b''.join(rows)


def prepare_level(folder):
    fixture,recipe=prepare_gunner(folder)
    recipe.update(frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
                  replay={'board':90,'aim':[100,104],'route_on':120,'save':SAVE_FRAMES},
                  load_replay={'fire':[20,34],'pre_exit_probe':40,'exit':60},load_setup_frames={},
                  scope='Ordinary NPC-driver/player-gunner save and fresh-boot continuation; no campaign traversal or visual claim')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());(folder/'load-replay.bin').write_bytes(replay(True))
    return fixture,recipe


def live_probe(monitor,mapping):
    x={name:words(monitor,address(mapping,name),size) for name,size in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return x


def saved_rows(payload,recipe):
    data=component(payload,11)
    if len(data)<40 or data[:4]!=b'RFNS':raise RuntimeError('Missing RFNS2 seat contract')
    version,size,count=struct.unpack_from('<III',data,4)
    if (version,count)!=(2,1) or len(data)!=16+size+24:raise RuntimeError('Wrong RFNS2 header/row count')
    if struct.unpack_from('<6I',data,16+size)!=(ACTOR,HOST,recipe['seat']['index'],1,1,1):
        raise RuntimeError('RFNS2 omitted explicit active driver/player-gunner control marker')
    vehicle=data[16:16+size]
    if len(vehicle)!=200 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<II',vehicle,4)!=(3,160) or struct.unpack_from('<II',vehicle,16)!=(3,3):
        raise RuntimeError('Saved Jeep is not alive and player occupied')
    if struct.unpack_from('<I',vehicle,108)[0]!=1:raise RuntimeError('Saved Jeep player lost gunner role')
    if vehicle[160:164]!=b'RFVR':raise RuntimeError('Missing retained authored route')
    route=list(struct.unpack_from('<9I',vehicle,164))
    if route!=[3,ROUTE,0,0,0,1,0,1,1]:raise RuntimeError(f'Wrong active route: {route}')
    player=component(payload,1)
    if len(player)!=544 or player[:4]!=b'RFPL' or struct.unpack_from('<I',player,4)[0]!=1:
        raise RuntimeError('Missing ordinary seated player record')
    npc=component(payload,2)
    if len(npc)<664 or npc[:4]!=b'RFNC' or struct.unpack_from('<I',npc,16)[0]!=1:raise RuntimeError('Expected one NPC driver')
    row=npc[64:]
    if struct.unpack_from('<I',row)[0]!=ACTOR or struct.unpack_from('<I',row,8)[0] or struct.unpack_from('<f',row,20)[0]<=0 or struct.unpack_from('<i',row,524)[0]!=13:
        raise RuntimeError('Saved authored driver is not living/seated')
    read=lambda blob,at,n:list(struct.unpack_from('<'+str(n)+'I',blob,at))
    result=dict(host_position=read(vehicle,32,3),host_velocity=read(vehicle,80,3),
                player_position=read(player,32,3),npc_position=read(row,28,3),
                aim=read(vehicle,112,2),exit_body=read(player,44,3),exit_eye=read(player,56,3),
                ammo=struct.unpack_from('<I',vehicle,104)[0],route=route)
    if not any(abs(f(x))>.0001 for x in result['aim']):raise RuntimeError('Fixture did not establish nondefault gunner aim')
    if not all(math.isfinite(f(x)) for key,value in result.items() if key not in ('ammo','route') for x in value):
        raise RuntimeError('Nonfinite saved pose/aim')
    return result


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    check_driver(result['extra'],teardown=True)


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES);x=source['extra'];state=source['checkpoint_state'];row=saved_rows(payload,recipe)
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary source save failed: {state}')
    g=x['rf_scene_jeep_npc_gunner'];npc,host=check_driver(x,teardown=True)
    if g[:2]!=[1,0] or g[4:9]!=[1,host,npc,npc,g[9]] or g[9] in (npc,host,0xffffffff) or g[10:16]!=[host,1,1,0,0,1]:
        raise RuntimeError('Source did not retain separate living NPC driver/player gunner')
    if x['rf_scene_apc_primary'][1] or any(x['rf_scene_jeep_gunner_restore']):raise RuntimeError('Source was not a quiet fresh capture')
    if x['rf_scene_setup_result']!=[2,ROUTE,28,0]:raise RuntimeError('Source setup events did not execute once')
    return row


def validate(source,loaded,payload,recipe):
    row=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    x=loaded['extra'];state=loaded['checkpoint_state'];p=x['rf_scene_jeep_gunner_restore'];live=loaded['probe']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary gunner restore failed: {state}')
    if p[:3]!=[1,HOST,ACTOR] or len(set(p[3:6]))!=3 or 0xffffffff in p[3:6] or p[6:11]!=[1,p[4],p[4],p[5],p[3]] or p[35:]!=[1,1,0]:
        raise RuntimeError(f'Composed ownership did not restore exactly once: {p}')
    if p[11:15]!=[1,0,ROUTE,row['ammo']]:raise RuntimeError('Restore lost route cursor or ammunition')
    expected=row['host_position']+row['host_velocity']+row['player_position']+row['npc_position']+row['aim']+row['exit_body']+row['exit_eye']
    if p[15:35]!=expected:raise RuntimeError('Composed restore changed saved pose, motion, aim or exit look before simulation')
    if not 40<=live['frame']<60:raise RuntimeError('Missed loaded gunner pre-exit probe')
    npc,host=check_driver(live);g=live['rf_scene_jeep_npc_gunner']
    if g[0:2]!=[0,0] or g[4:9]!=[1,host,npc,npc,p[5]] or g[10:16]!=[host,1,1,0,0,1]:
        raise RuntimeError('Restored player did not remain gunner without reboarding')
    if live['rf_scene_apc_primary'][1]<1 or live['rf_scene_apc_primary'][7]>=row['ammo']:
        raise RuntimeError('Restored gunner did not fire/debit real mounted rounds')
    end=x['rf_scene_jeep_npc_gunner']
    if end[0:2]!=[0,1] or end[4:9]!=[0,host,npc,npc,0xffffffff] or end[10:16]!=[0xffffffff,0,1,0,0,1]:
        raise RuntimeError('Restored gunner safe exit did not preserve real NPC driver')
    if any(x['rf_scene_setup_result']) or x['rf_scene_vehicle_route_state'][5]!=ROUTE or not x['rf_scene_vehicle_route_state'][3]:
        raise RuntimeError('Route failed to continue or replayed a setup event')
    if x['rf_scene_npc_seat_checkpoint'][:4]!=[0,1,1,1]:raise RuntimeError('RFNS driver did not restore exactly once')
    return dict(result='PASS',saved=row,composed_restore=p,loaded_fire=live['rf_scene_apc_primary'][1],
                no_reboarding_or_route_replay=True,safe_exit_preserved_driver=True,
                limitations='One ordinary fresh-boot gunner continuation; current in-session gunner loading, mounted-flight saves, corpse driver, visuals and campaign traversal excluded.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);parser.add_argument('--resume-saved-run',type=Path,help='Continue from a retained ordinary source save on the same harness HDD');args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-jeep-gunner-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes());(DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,ROUTE));(DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve()
            source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes()
            report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
            payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=source;report['saved']=validate_source(source,payload,recipe)
        (DISC/'campaign-setup.bin').unlink();(DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay(True))
        build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,extra_symbols=SYMBOLS,allow_guest_error=True,probe=live_probe,probe_frame=40)
        report['phases']['load']=loaded;report.update(validate(source,loaded,payload,recipe))
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
