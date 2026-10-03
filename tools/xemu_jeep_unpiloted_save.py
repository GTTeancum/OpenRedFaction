"""Ordinary Xbox save with player gunner retained after original Jeep driver death.

Source: grounded real miner7646/Jeep7629, STOP0, Use90, Slay120,
quiet occupied gunner save240. Freshload probes40, transfers by normal cycle60,
throttles80..109, exits150 and finishes180. No death/setup replay, forced
ownership or pose, fabricated save bytes, images or host input.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_jeep_driver_transfer import prepare_level as prepare_transfer,SYMBOLS as TRANSFER_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST,SLAY,U,f
from xemu_npc_jeep_detached_save import STOP,component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES,LOAD_FRAMES=240,180
NONE=0xffffffff
SYMBOLS=dict(TRANSFER_SYMBOLS,rf_scene_jeep_gunner_restore=38,
    rf_scene_npc_seat_checkpoint=6,rf_scene_world_load_reject=3,
    rf_scene_npc_checkpoint_reject_state=6,rf_scene_vehicle_restore_trace=8,
    rf_scene_checkpoint_world_reject=9,rf_scene_world_player_probe=3,
    rf_scene_npc_jeep_detached_probe=16,rf_scene_live_corpses=8,
    rf_scene_corpse_checkpoint=8,rf_scene_live_death_audio=4)


def replay(load=False):
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,
        float(load and 80<=frame<110),0,0,0,0,
        int(frame==(150 if load else 90)),0,0,int(load and frame==60),0)
        for frame in range(LOAD_FRAMES if load else SAVE_FRAMES))


def prepare_level(folder):
    path,recipe=prepare_transfer(folder);folder=Path(folder)
    recipe.update(scope=__doc__,frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
        replay={'board_gunner':90,'probe_living_driver':100,'slay_driver':120,'save_unpiloted_gunner':240},
        load_replay={'probe_unpiloted_gunner':40,'transfer_to_driver':60,'throttle':[80,109],'exit':150},
        load_setup_frames={},limitations='One parked former-driver gunner save, fresh-load transfer and ordinary throttle/exit; moving gunner saves, corpse retirement and audiovisual fidelity are outside this check.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());(folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    x={n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return x


def saved_rows(payload,recipe):
    block=component(payload,11);lifetime=None
    if block[:4]==b'RFCL':
        version,size,count=struct.unpack_from('<3I',block,4)
        if (version,count)!=(1,1) or len(block)!=16+size+48:raise RuntimeError('Invalid single-corpse lifetime wrapper')
        lifetime=list(struct.unpack_from('<12I',block,16+size))
        if lifetime[0]!=ACTOR:raise RuntimeError('Corpse lifetime lost original driver UID')
        block=block[16:16+size]
    if block[:4]!=b'RFNS':raise RuntimeError('Missing former driver/player-gunner RFNS3')
    version,size,count=struct.unpack_from('<3I',block,4)
    if (version,count)!=(3,1) or len(block)!=16+size+24:raise RuntimeError('Expected single RFNS3 row')
    seat=list(struct.unpack_from('<6I',block,16+size))
    if seat!=[ACTOR,HOST,recipe['seat']['index'],1,0,1]:raise RuntimeError(f'Unpiloted gunner relation not saved: {seat}')
    vehicle=block[16:16+size]
    if len(vehicle)!=200 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<2I',vehicle,4)!=(3,160) or struct.unpack_from('<2I',vehicle,16)!=(3,3) or struct.unpack_from('<I',vehicle,108)[0]!=1:
        raise RuntimeError('Expected occupied live gunner Jeep RFVC3')
    if vehicle[160:164]!=b'RFVR' or struct.unpack_from('<9I',vehicle,164)!=(3,0,0,0,0,0,1,0,0):
        raise RuntimeError('Parked AI mode changed or route appeared')
    npc=component(payload,2)
    if npc[:4]!=b'RFNC' or len(npc)<664 or struct.unpack_from('<I',npc,4)[0]!=10 or struct.unpack_from('<I',npc,16)[0]!=1:
        raise RuntimeError('Expected RFNC10 original dead driver')
    row=npc[64:];uid,_,retired,flags=struct.unpack_from('<4I',row)
    health=struct.unpack_from('<f',row,20)[0]
    if uid!=ACTOR or retired or flags&0x4000 or not math.isfinite(health) or health>0 or struct.unpack_from('<i',row,524)[0]==13 or struct.unpack_from('<I',row,552)[0]!=1:
        raise RuntimeError('Former driver did not save as a detached terminal corpse')
    player=component(payload,1)
    if player[:4]!=b'RFPL' or len(player)!=544 or struct.unpack_from('<I',player,4)[0]!=1:
        raise RuntimeError('Missing ordinary mounted player RFPL1')
    read=lambda b,at,n:list(struct.unpack_from('<'+str(n)+'I',b,at))
    return dict(seat=seat,health=health,corpse_lifetime=lifetime,
        host_position=read(vehicle,32,3),host_velocity=read(vehicle,80,3),
        player_position=read(player,32,3),npc_position=read(row,28,3),
        aim=read(vehicle,112,2),exit_body=read(player,44,3),exit_eye=read(player,56,3),
        ammo=struct.unpack_from('<I',vehicle,104)[0])


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0 or result['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB continuation')
    x=result['extra']
    if x['rf_scene_vehicle_state'][5] or x['rf_scene_npc_seats'][9] or x['rf_scene_vehicle_route_state'][0] or x['rf_scene_vehicle_route_state'][3] or x['rf_scene_vehicle_route_state'][7] or x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7] or x['rf_scene_apc_primary'][1]:
        raise RuntimeError('Unexpected route/fire or vehicle/seat error')


def unpiloted(x):
    g=x['rf_scene_jeep_npc_gunner'];host,player=g[5],g[9]
    if host==player or NONE in (host,player) or g[4:9]!=[1,host,NONE,NONE,player] or g[10:16]!=[host,1,0,0,0,1]:
        raise RuntimeError(f'Empty driver/player gunner ownership not retained: {g}')
    if x['rf_scene_jeep_seats'][4]!=1 or x['rf_scene_npc_seats'][6]:raise RuntimeError('Former driver reseated or player promoted implicitly')
    return host,player


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES);pre,x=source['probe'],source['extra'];state=source['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary unpiloted gunner save failed: {state}')
    if not 100<=pre['frame']<120 or pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or pre['rf_scene_npc_seats'][6]!=1 or pre['rf_scene_jeep_npc_gunner'][12]!=1:
        raise RuntimeError('Player did not board beside the original living driver before Slay')
    slay=x['rf_scene_script_slays']
    if x['rf_scene_setup_result']!=[2,SLAY,1,0] or slay[:3]!=[1,1,ACTOR] or f(slay[3])>0 or slay[5] or x['rf_scene_npc_seats'][5:9]!=[1,0,ACTOR,HOST]:
        raise RuntimeError('Ordinary driver death/detachment failed')
    unpiloted(x)
    if x['rf_scene_vehicle_state'][1:4]!=[1,0,1] or x['rf_scene_jeep_driver_transfer'][0] or any(x['rf_scene_jeep_gunner_restore']):
        raise RuntimeError('Source exited, promoted player or restored synthetic control')
    saved=saved_rows(payload,recipe)
    if saved['ammo']!=x['rf_scene_apc_primary'][7] or saved['host_position']!=x['rf_scene_vehicle_state'][6:9]:raise RuntimeError('Saved Jeep differs from real source owner')
    return saved


def validate(source,loaded,payload,recipe):
    row=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    pre,x=loaded['probe'],loaded['extra'];state=loaded['checkpoint_state'];p=x['rf_scene_jeep_gunner_restore']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):raise RuntimeError('Ordinary RFNS3 load failed')
    host,player=unpiloted(pre)
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_jeep_npc_gunner'][:2]!=[0,0] or pre['rf_scene_jeep_driver_transfer'][0]:
        raise RuntimeError('Load reboarded or implicitly promoted player before control input')
    if p[:3]!=[1,HOST,ACTOR] or p[3]!=host or p[5]!=player or len(set(p[3:6]))!=3 or NONE in p[3:6] or p[6:11]!=[1,NONE,NONE,player,host] or p[11:15]!=[0,0,0,row['ammo']] or p[35:]!=[1,0,0] or pre['rf_scene_jeep_gunner_restore']!=p:
        raise RuntimeError(f'RFNS3 composition lost former driver identity or empty ownership: {p}')
    expected=row['host_position']+row['host_velocity']+row['player_position']+row['npc_position']+row['aim']+row['exit_body']+row['exit_eye']
    if p[15:35]!=expected:raise RuntimeError('Restore changed saved pose/motion/aim/exit orientation')
    if x['rf_scene_npc_seat_checkpoint'][:4]!=[0,1,1,0] or x['rf_scene_npc_seat_checkpoint'][5]:raise RuntimeError('Inactive driver relation did not restore once')
    t=x['rf_scene_jeep_driver_transfer']
    if t[:6]!=[1,0,0,1,host,player] or x['rf_scene_vehicle_state'][1:4]!=[0,1,0] or x['rf_scene_jeep_seats'][3]!=1 or not x['rf_scene_jeep_seats'][1]:
        raise RuntimeError(f'Ordinary transfer/throttle/exit failed: {t}')
    g=x['rf_scene_jeep_npc_gunner']
    if g[0] or g[2] or g[6:9]!=[NONE]*3 or g[10]!=NONE:raise RuntimeError('Transfer reboarded or exit retained control ownership')
    distance=math.sqrt(sum((f(x['rf_scene_vehicle_state'][6+i])-f(t[6+i]))**2 for i in (0,2)))
    if not math.isfinite(distance) or distance<=.1:raise RuntimeError('Player throttle failed to move actual Jeep after transfer')
    if x['rf_scene_apc_primary'][7]!=row['ammo'] or any(x['rf_scene_setup_result']) or any(x['rf_scene_script_slays']) or any(x['rf_scene_live_death_audio']):
        raise RuntimeError('Load spent ammunition or replayed setup/death effects')
    return dict(result='PASS',saved=row,restore=p,transfer=t,moved_horizontal=distance,
        restored_empty_driver=True,ordinary_transfer_and_exit=True,limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);parser.add_argument('--resume-saved-run',type=Path,help='Continue from a retained ordinary source save on the same harness HDD');args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('jeep-unpiloted-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',phases={},recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes());(DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,SLAY));(DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve()
            source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes()
            report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True,probe=live_probe,probe_frame=100)
            report['phases']['save']=source
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
