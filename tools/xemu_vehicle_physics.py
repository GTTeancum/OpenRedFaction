"""Bounded Xbox occupied Jeep freeze, ordinary save/fresh-load and wake.

Real L12S1 Jeep7629/miner7646 in grounded CTF06. Use90 boards; ordinary
Follow_Waypoints120 starts movement, Turn_Off_Physics150 freezes the chassis
with both occupants retained. Save240; fresh-load probe40 remains frozen, Invert120 wakes
the retained route. No host input, images, forced damage or state injection.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_npc_jeep_gunner import prepare_level as prepare_pair,check_driver,SYMBOLS as GUNNER_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST,ROUTE,f
from xemu_npc_jeep_detached_save import STOP,component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

START,FREEZE,WAKE,FREEZE_TIMER=915100,915101,915102,915103
SAVE_FRAMES,LOAD_FRAMES=240,180
SYMBOLS=dict(GUNNER_SYMBOLS,rf_scene_vehicle_physics=16,
             rf_scene_vehicle_physics_apply=24,rf_scene_physics_state=8,
             rf_scene_npc_seat_checkpoint=6,rf_scene_world_load_reject=3,
             rf_scene_checkpoint_world_reject=9,rf_scene_actor_landing=8,
             rf_scene_npc_checkpoint_reject_state=6,rf_scene_vehicle_physics_restore=24,
             rf_scene_vehicle_physics_checkpoint=8)


def replay(load=False):
    count=LOAD_FRAMES if load else SAVE_FRAMES
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(not load and frame==90),0,0,0,0) for frame in range(count))


def prepare_level(folder):
    path,recipe=prepare_pair(folder)
    raw=read_entry(path,'L12S1.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L12S1.rfl'))
    section=next(s for s in meta['sections'] if s['type']=='0x600')
    # Reuse the real fixture's Follow_Waypoints record with its one-second
    # delay and ordinary path references, preserving the existing route.
    events=raw[section['offset']+12:section['offset']+8+section['size']]
    stop=event(STOP,'Set_AI_Mode','park_for_gunner',(HOST,))
    if not events.startswith(stop):raise RuntimeError('Grounded fixture event layout changed')
    follow=events[len(stop):]
    events=U(6)+stop+follow+event(START,'Delay','start_and_freeze',(ROUTE,FREEZE_TIMER))+\
        command(FREEZE_TIMER,'Delay','freeze_after_motion',(FREEZE,),delay=1.5)+\
        event(FREEZE,'Turn_Off_Physics','freeze_occupied_jeep',(HOST,))+\
        command(WAKE,'Invert','wake_saved_jeep',(FREEZE,),delay=1.0)
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=events if kind==0x600 else raw[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L12S1.rfl'))
    archive(path,[('L12S1.rfl',out)])
    recipe.update(scope=__doc__,frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
                  setup_frames={str(STOP):0,str(START):60},
                  timeline={'board':90,'route':120,'freeze':150,'save':SAVE_FRAMES},
                  load_timeline={'preload_park':0,'frozen_probe':40,'wake_request':60,'wake':120,'end':LOAD_FRAMES},
                  staged_fields=['authored entity transforms','ordinary route/Set_AI_Mode/Turn_Off_Physics/Invert event graph'],
                  limitations='One occupied Jeep with living NPC driver. Original class bit1000 forced release applies to Driller, not Jeep. No Driller exit, passive vehicles, immunity, NPC freeze persistence or audiovisual claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    (folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    values={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    values['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return values


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    check_driver(result['extra'],teardown=True)


def check_occupied(values,entries):
    pair=values['rf_scene_npc_seat_probe'];npc,host=pair[2:4]
    g=values['rf_scene_jeep_npc_gunner'];player=g[9]
    if player in (npc,host,0xffffffff) or g[:3]!=[entries,0,0] or g[4:9]!=[1,host,npc,npc,player] or g[10:16]!=[host,1,1,0,0,1]:
        raise RuntimeError(f'Jeep did not preserve real player gunner/NPC driver ownership: {g}')
    if values['rf_scene_vehicle_state'][1:4]!=[entries,0,1] or values['rf_scene_apc_primary'][1]:
        raise RuntimeError('Unexpected boarding/exit or firing during neutral occupied freeze fixture')


def saved_rows(payload,recipe):
    data=component(payload,11)
    if data[:4]!=b'RFNS' or len(data)<40:raise RuntimeError('Missing authored seat wrapper')
    version,size,count=struct.unpack_from('<III',data,4)
    if (version,count)!=(2,1) or len(data)!=16+size+24:raise RuntimeError('Wrong occupied-player RFNS2 contract')
    if struct.unpack_from('<6I',data,16+size)!=(ACTOR,HOST,recipe['seat']['index'],1,1,1):
        raise RuntimeError('Frozen save lost its living NPC driver or player gunner')
    data=data[16:16+size]
    if data[:4]!=b'RFPV':raise RuntimeError('Frozen Jeep omitted physics-state wrapper')
    version,size,count=struct.unpack_from('<III',data,4)
    if (version,count)!=(1,1) or len(data)!=16+size+72:raise RuntimeError('Wrong RFPV1 active-owner row')
    physics=list(struct.unpack_from('<18I',data,16+size))
    if physics[:3]!=[HOST,0,1] or physics[3]&0x98000000!=0x18000000 or physics[4] or physics[5]&~0x06000000 or physics[6] or physics[7]>1 or physics[17]:
        raise RuntimeError(f'Invalid saved frozen-owner state: {physics}')
    if any(physics[8:11]) or not all(math.isfinite(f(v)) for v in physics[8:17]):
        raise RuntimeError('Frozen RFPV velocity nonzero or force/torque nonfinite')
    vehicle=data[16:16+size]
    if len(vehicle)!=200 or vehicle[:4]!=b'RFVC' or struct.unpack_from('<II',vehicle,4)!=(3,160) or struct.unpack_from('<II',vehicle,16)!=(3,3) or struct.unpack_from('<I',vehicle,108)[0]!=1:
        raise RuntimeError('Expected living player-gunner occupied RFVC3 Jeep')
    if vehicle[160:164]!=b'RFVR':raise RuntimeError('Frozen save omitted retained route')
    route=list(struct.unpack_from('<9I',vehicle,164))
    if route!=[3,ROUTE,0,0,0,1,0,1,1]:raise RuntimeError(f'Frozen save altered route state: {route}')
    return dict(physics=physics,position=list(struct.unpack_from('<3I',vehicle,32)),
                velocity=list(struct.unpack_from('<3I',vehicle,80)),route=route)


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES)
    x=source['extra'];pre=source['probe'];state=source['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary save failed: {state}')
    if not 140<=pre['frame']<150:raise RuntimeError('Missed mounted moving pre-freeze probe')
    check_driver(pre)
    check_occupied(pre,1);check_occupied(x,1)
    if pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or not any(abs(f(v))>.01 for v in pre['rf_scene_vehicle_state'][9:12]):
        raise RuntimeError('Player never boarded a naturally moving Jeep before freeze')
    physics=x['rf_scene_vehicle_physics'];applied=x['rf_scene_vehicle_physics_apply'];vehicle=x['rf_scene_vehicle_state']
    if physics[:8]!=[1,1,0,0,0,0,0,0] or physics[8]!=HOST or physics[10] or physics[13] or physics[14:]!=[1,0]:
        raise RuntimeError(f'Jeep freeze incorrectly requested Driller-only player release: {physics}')
    if applied[:2]!=[1,HOST] or applied[3] or applied[23]!=1 or not 2480<=applied[4]<=2520:
        raise RuntimeError('Freeze callback did not act on the occupied Jeep at frame150')
    if applied[6]&0x98000000!=0x18000000 or applied[10]&0x800 or any(applied[17:23]):
        raise RuntimeError('Freeze flags/zeroed velocity and momentum differ')
    if not any(abs(f(v))>.01 for v in applied[14:17]):raise RuntimeError('Freeze did not stop real nonzero motion')
    if vehicle[1:4]!=[1,0,1] or vehicle[6:9]!=applied[11:14] or any(vehicle[9:12]):
        raise RuntimeError('Frozen chassis drifted or lost player ownership')
    if x['rf_scene_setup_result']!=[2,START,48,0]:raise RuntimeError('Source ordinary event chain failed')
    row=saved_rows(payload,recipe)
    if row['position']!=vehicle[6:9] or any(row['velocity']):raise RuntimeError('Saved frozen physical pose differs')
    return row


def validate(source,loaded,payload,recipe):
    row=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    x=loaded['extra'];pre=loaded['probe'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError('Ordinary frozen-vehicle restore failed')
    if not 40<=pre['frame']<60:raise RuntimeError('Missed restored frozen window before wake request')
    check_driver(pre)
    check_occupied(pre,0);check_occupied(x,0)
    if pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_vehicle_state'][6:9]!=row['position'] or any(pre['rf_scene_vehicle_state'][9:12]):
        raise RuntimeError('Fresh load woke/drifted or reboarded frozen Jeep')
    if any(pre['rf_scene_vehicle_physics'][:8]):raise RuntimeError('Restore replayed freeze/exit callbacks')
    restore=x['rf_scene_vehicle_physics_restore'];saved=row['physics']
    if restore[:5]!=[1,HOST,saved[2],saved[3],saved[4]] or restore[5]&0x06000000!=saved[5] or restore[6]&0x800!=saved[6] or restore[7:17]!=saved[7:17] or restore[17]:
        raise RuntimeError(f'Immutable RFPV restore differs from saved row: {restore}')
    checkpoint=x['rf_scene_vehicle_physics_checkpoint']
    if checkpoint[:3]!=[0,0,1] or checkpoint[3:6]!=saved[2:5] or checkpoint[6:]!=[0,0]:
        raise RuntimeError(f'RFPV did not assign exactly once: {checkpoint}')
    p=x['rf_scene_vehicle_physics'];a=x['rf_scene_vehicle_physics_apply']
    if p[:8]!=[1,0,1,0,0,0,0,0] or p[8]!=HOST or p[10]!=1 or p[14:]!=[0,0]:
        raise RuntimeError(f'Ordinary OFF did not wake restored Jeep: {p}')
    if a[:2]!=[1,HOST] or a[3]!=1 or a[5]&0x98000000!=0x18000000 or not a[6]&0x80000000 or a[10]&0x800:
        raise RuntimeError('Restored frozen flags or OFF wake bits differ, or Jeep gained Driller-only814800')
    if not 1980<=a[4]<=2020:raise RuntimeError('OFF did not execute at frame120 after fresh-load restoration')
    displacement=math.dist([f(v) for v in row['position']],[f(v) for v in x['rf_scene_vehicle_state'][6:9]])
    if displacement<.1 or x['rf_scene_vehicle_route_state'][3]<=pre['rf_scene_vehicle_route_state'][3]:
        raise RuntimeError('Retained route did not resume actual chassis movement')
    if x['rf_scene_setup_result']!=[2,WAKE,3,0] or x['rf_scene_vehicle_route_state'][5]!=ROUTE or x['rf_scene_vehicle_state'][1:4]!=[0,0,1]:
        raise RuntimeError('Load reissued route/boarding or lost original route identity')
    return dict(result='PASS',saved=row,jeep_occupants_retained=True,source_exit_count=source['extra']['rf_scene_vehicle_physics'][5],
                loaded_displacement=displacement,no_reboard_or_route_replay=True,limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--resume-saved-run',type=Path)
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-physics-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',recipe=recipe,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,START));(DISC/'player-replay.bin').write_bytes(replay())
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve();source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes();report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=140,allow_guest_error=True)
            report['phases']['save']=source;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=source;report['saved']=validate_source(source,payload,recipe)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        # Frame0 setup precedes world-load publication. Its immediate STOP
        # is overwritten by saved RFVR; request delayed WAKE at60 only after
        # restore, so loading the saved pending-event queue cannot discard it.
        # Fixture RFL/identity stays byte-identical to the retained source.
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,WAKE));(DISC/'player-replay.bin').write_bytes(replay(True))
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
