"""Bounded Xbox switched-Jeep ordinary save/fresh-boot continuation.

Reuse the successful two-Jeep CTF06 fixture unchanged. Source350 spends each
vehicle's own ammunition and saves while B is occupied. Freshload140 retains
B, then ordinary exit60/Use100 returns to parked A without setup or reboarding
B. No state injection, images, host input, route or campaign traversal.
"""
import argparse
import datetime
import json
from pathlib import Path
import struct

from xemu_vehicle_switch import A,B,SYMBOLS as SWITCH_SYMBOLS,prepare_level as prepare_pair,replay as pair_replay
from xemu_npc_jeep_detached_save import component
from xemu_turret_combat import f
from build_fragment_platform_fixture import U
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES,LOAD_FRAMES=350,140
SYMBOLS=dict(SWITCH_SYMBOLS,rf_scene_world_load_reject=3,
             rf_scene_checkpoint_world_reject=9,rf_scene_vehicle_restore_trace=8,
             rf_scene_npc_checkpoint_reject_state=6,rf_scene_vehicle_switch_restore=16)


def replay(load=False):
    if not load:return pair_replay()[:8+SAVE_FRAMES*48]
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(frame in (60,100)),0,0,0,0) for frame in range(LOAD_FRAMES))


def prepare_level(folder):
    path,recipe=prepare_pair(folder)
    recipe.update(scope=__doc__,frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
                  replay={'board_A':90,'gunner_A':110,'fire_A':[130,149],'probe_A':190,'exit_A':210,
                          'board_B':250,'gunner_B':270,'fire_B':[290,297],'save_occupied_B':SAVE_FRAMES},
                  load_replay={'probe_occupied_B':40,'exit_B':60,'return_A':100},
                  limitations='One same-class switched Jeep save/fresh-boot continuation; no current-session load, NPC seats, cross-profile handoff or audiovisual verification.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());(folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    x={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return x


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    x=result['extra']
    if x['rf_scene_vehicle_state'][5] or x['rf_scene_vehicle_route_state'][0] or x['rf_scene_vehicle_route_state'][3] or any(x['rf_scene_npc_seats']) or any(x['rf_scene_setup_result']):
        raise RuntimeError('Unexpected vehicle error, NPC seat or authored setup replay')
    if x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7]:raise RuntimeError('Unexpected NPC combat')


def saved_rows(payload):
    data=component(payload,11)
    if len(data)<344 or data[:4]!=b'RFSW':raise RuntimeError('Missing RFSW switched-owner wrapper')
    version,size,uid,profile,count=struct.unpack_from('<5I',data,4)
    if (version,uid,profile,count)!=(1,B,3,1) or len(data)!=24+size+320:
        raise RuntimeError('Wrong active UID/profile or parked-owner RFSW1 contract')
    bank=data[24+size:];inner=data[24:24+size]
    if struct.unpack_from('<I',bank)[0]!=A:raise RuntimeError('Parked RFSW row no longer names original Jeep')
    if len(inner)<96 or inner[:4]!=b'RFVA':raise RuntimeError('Missing ordinary passive Jeep state')
    version,host_bytes,passive_count=struct.unpack_from('<3I',inner,4)
    if (version,passive_count)!=(2,1) or len(inner)!=16+host_bytes+80:
        raise RuntimeError('Wrong RFVA2 passive-owner contract')
    vehicle=inner[16:16+host_bytes];passive=inner[16+host_bytes:]
    if struct.unpack_from('<2I',passive)!=(A,0):raise RuntimeError('Saved parked Jeep is not original unparented owner')
    if len(vehicle)!=160 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<II',vehicle,4)!=(3,160) or struct.unpack_from('<II',vehicle,16)!=(3,3) or struct.unpack_from('<I',vehicle,108)[0]!=1:
        raise RuntimeError('Active switched Jeep lost live occupied gunner state')
    b=list(struct.unpack('<80I',bank));v=list(struct.unpack('<40I',vehicle))
    p=list(struct.unpack('<20I',passive))
    if b[1]!=1 or b[3] or b[8] or b[15] or b[79]:
        raise RuntimeError('Parked control bank is not valid, unfrozen and unoccupied')
    if b[16:28]!=p[2:14] or p[19]:
        raise RuntimeError('Parked control pose disagrees with its living RFVA owner')
    return dict(active_uid=uid,active_ammo=v[26],active_rng=v[30],
                active_position=v[8:11],parked_uid=b[0],parked_ammo=b[4],
                parked_rng=b[6],parked_position=p[2:5])


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES);pre=source['probe'];x=source['extra'];state=source['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary switched save failed: {state}')
    if not 190<=pre['frame']<210 or pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][1]<1:
        raise RuntimeError('Missing first vehicle mounted/fire evidence')
    a_ammo=pre['rf_scene_apc_primary'][7];b_ammo=x['rf_scene_apc_primary'][7]
    sw=x['rf_scene_vehicle_switch'];history=x['rf_scene_vehicle_switch_history'];first=history[:32]
    if sw[1]!=1 or sw[4:6]!=[A,B] or sw[8] or sw[15]!=B or first[0:4]!=[A,B,pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12]] or any(history[32:]):
        raise RuntimeError('Source did not perform exactly one identity-preserving A-to-B handoff')
    if first[8]!=a_ammo or first[9]<=b_ammo or first[9]<=a_ammo or a_ammo==b_ammo:
        raise RuntimeError('Source owners did not independently spend distinct ammunition')
    if x['rf_scene_vehicle_state'][1:4]!=[2,1,1] or x['rf_scene_jeep_seats'][4]!=1 or x['rf_scene_apc_primary'][1]<=pre['rf_scene_apc_primary'][1]:
        raise RuntimeError('Source is not a quiet mounted second gunner after real firing')
    saved=saved_rows(payload)
    if saved['active_uid']!=B or saved['active_ammo']!=b_ammo or saved['parked_uid']!=A or saved['parked_ammo']!=a_ammo:
        raise RuntimeError('Saved codec lost active identity or either owner ammunition')
    if saved['active_position']!=x['rf_scene_vehicle_state'][6:9] or saved['parked_position']!=first[12:15]:
        raise RuntimeError('Saved active/parked positions differ from their actual owners')
    return saved


def validate(source,loaded,payload,recipe):
    saved=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    x=loaded['extra'];pre=loaded['probe'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError('Fresh ordinary switched-vehicle load failed')
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][7]!=saved['active_ammo']:
        raise RuntimeError('Saved second Jeep was not restored occupied with exact ammunition')
    if pre['rf_scene_apc_primary'][1] or pre['rf_scene_vehicle_switch'][1]:raise RuntimeError('Load replayed firing or ordinary Use handoff')
    restored=x['rf_scene_vehicle_switch_restore']
    expected=[1,B,3,1,1,A,saved['parked_ammo'],saved['parked_rng'],0,0,0,
              saved['active_ammo'],saved['active_rng'],pre['rf_scene_vehicle_state'][12],
              x['rf_scene_vehicle_state'][12],0]
    if restored!=expected or pre['rf_scene_vehicle_switch_restore']!=restored:
        raise RuntimeError(f'Exact active/parked bank restore or stable handle assignment failed: {restored}')
    sw=x['rf_scene_vehicle_switch'];apply=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:6]!=[B,A] or sw[8] or sw[9]!=1 or sw[15]!=A or sw[10:12]!=[saved['active_ammo'],saved['parked_ammo']]:
        raise RuntimeError(f'Ordinary return lost saved parked owner or either ammo count: {sw}')
    if x['rf_scene_vehicle_state'][1:4]!=[1,1,1] or x['rf_scene_apc_primary'][7]!=saved['parked_ammo'] or x['rf_scene_apc_primary'][1]:
        raise RuntimeError('Fresh-load exit/return ownership or first vehicle ammo failed')
    if apply[:4]!=[B,A,pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12]] or apply[8:10]!=[saved['active_ammo'],saved['parked_ammo']] or apply[15:18]!=saved['parked_position'] or apply[24:30]!=[1,0,0,0,0,1]:
        raise RuntimeError('Restored bank did not preserve exact parked pose, role and registry identity')
    if not 99<=apply[30]<=101:raise RuntimeError('Return did not result from the scheduled ordinary Use input')
    if [f(v) for v in apply[4:8]]!=[recipe['details']['authored_health']]*2+[recipe['details']['authored_armor']]*2:
        raise RuntimeError('Save/handoff changed either authored vehicle vitals')
    return dict(result='PASS',saved=saved,restore=restored,return_exchange=apply,
                no_reboard_of_restored_vehicle=True,parked_ammo_retained=True,limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--resume-saved-run',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-switch-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',phases={},recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve();source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes();report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,600,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=190,allow_guest_error=True)
            report['phases']['save']=source;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=source;report['saved']=validate_source(source,payload,recipe)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(True))
        build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,extra_symbols=SYMBOLS,
            probe=live_probe,probe_frame=40,allow_guest_error=True)
        report['phases']['load']=loaded;report.update(validate(source,loaded,payload,recipe))
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
