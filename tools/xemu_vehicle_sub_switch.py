"""Bounded original submarine/Jeep Xbox switch and ordinary save continuation.

Uses real L10S4 sub7010 and Jeep7009 records in L5S3 parkinglot water/dock.
Only entity transforms and testbed setup change; liquid/world geometry remains
original. Initial ordinary swimming schedules require native admission proof.
Source boards/fires submarine, swims to dock, boards/fires Jeep and saves.
Freshload exits Jeep, swims back and Uses submarine; no forced state/host input.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry,U,F
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_turret_combat import entity_rows,f
from xemu_vehicle_mixed_types import metadata,diagnostics as mixed_diagnostics,SYMBOLS as MIXED_SYMBOLS
from xemu_npc_jeep_detached_save import component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SUB,JEEP=7010,7009
SAVE_FRAMES,LOAD_FRAMES=620,240
SYMBOLS=dict(MIXED_SYMBOLS,rf_scene_submarine_weapon=8,
    rf_scene_submarine_boarding=8,rf_scene_vehicle_switch_restore=16,
    rf_scene_vehicle_profile_packs=16,rf_scene_actor_pose=59,
    rf_scene_world_load_reject=3,rf_scene_checkpoint_world_reject=9,
    rf_scene_vehicle_restore_trace=8,rf_scene_vehicle_player_capture=8)
IDENTITY=(0,0,1,1,0,0,0,1,0)
SUB_POSITION=(104.,58.7,7.)
JEEP_POSITION=(110.5,61.+.75-.040839687+.01,7.)
SPAWN=(104.,58.7,10.5)


def replay(load=False):
    rows=[]
    for frame in range(LOAD_FRAMES if load else SAVE_FRAMES):
        if load:
            side=-float(80<=frame<111);rise=0;dive=int(115<=frame<175);use=frame in (60,200);fire=cycle=0
        else:
            side=float(230<=frame<310);rise=int(175<=frame<310);dive=0
            use=frame in (12,160,480);fire=int(frame==40 or 520<=frame<529);cycle=int(frame==500)
        rows.append(struct.pack('<5f7I',side,0,0,0,0,dive,rise,int(use),fire,0,cycle,0))
    return b'RFI6'+U(48)+b''.join(rows)


def prepare_level(folder):
    folder=Path(folder);source=read_entry(ROOT/'Installed_Game/levels2.vpp','L10S4.rfl')
    original=read_entry(ROOT/'Installed_Game/levels1.vpp','L5S3.rfl')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='L5S3.rfl'))
    records=[];owners=[]
    for uid,cls,pos,profile in ((SUB,'sub',SUB_POSITION,4),(JEEP,'Jeep01',JEEP_POSITION,3)):
        r=next(r for r in entity_rows(source) if r['uid']==uid);details=metadata(r)
        if r['name']!=cls or details['seat_host_uid']!=-1 or details['creation_flags']:
            raise RuntimeError('Expected original visible unparented L10S4 vehicle')
        raw=bytearray(r['raw']);at=r['transform'];raw[at:at+48]=F(*pos,*IDENTITY)
        if raw[:at]!=r['raw'][:at] or raw[at+48:]!=r['raw'][at+48:]:raise RuntimeError('Changed authored non-transform vehicle data')
        records.append(bytes(raw));owners.append(dict(uid=uid,profile=profile,class_name=cls,
            position=pos,metadata=details,source_sha256=hashlib.sha256(r['raw']).hexdigest()))
    # No scripts, moving groups, triggers, navigation, items or unrelated actors.
    # Original static clutter and world/liquid geometry remain.
    replacements={0x30000:U(2)+b''.join(records),0x600:U(0),0x3000:U(0),
        0x60000:U(0),0x40000:U(0),0x20000:U(0),0x10000:U(0),0x70000:F(*SPAWN,*IDENTITY)}
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        k=int(section['type'],16);payload=replacements.get(k,original[section['offset']+8:section['offset']+8+section['size']])
        offsets[k]=len(out);out+=U(k,len(payload))+payload
    if replacements.keys()-offsets.keys():raise RuntimeError('Original testbed lacks required replacement section')
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L10S4.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=records:raise RuntimeError('Vehicle metadata did not roundtrip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('L10S4.rfl',out)])
    recipe=dict(scope=__doc__,status='PREPARED_NOT_RUN',owners=owners,spawn=SPAWN,
        geometry=dict(source='levels1.vpp/L5S3.rfl',water_room=4,water_surface=60.9651985168457,
            dock_face=2359,dock_plane=[0,1,0,-61],dock_bounds=[[107.75,113.75],[2.25,12.25]],
            geometry_and_liquid_unchanged=True,sub_collision_radius=2),
        frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
        replay={'board_sub':12,'torpedo':40,'probe_sub':140,'exit_sub':160,
            'swim_up':[175,309],'approach_dock_right':[230,309],'board_Jeep':480,
            'gunner':500,'fire_Jeep':[520,528],'save_occupied_Jeep':SAVE_FRAMES},
        load_replay={'probe_occupied_Jeep':40,'exit_Jeep':60,'swim_left':[80,110],'crouch_dive':[115,174],'return_sub':200},
        limitations='Initial wet/dry approach schedule is unverified until real water and collision admission pass. One parked submarine/Jeep transition and ordinary save; no campaign progression, moving exchange, Fighter or audiovisual claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());(folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    x={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return x


def diagnostics(result):
    value=mixed_diagnostics(result)
    for name,sample in (('live',result.get('probe')),('final',result.get('extra'))):
        if not sample:continue
        pose=sample.get('rf_scene_actor_pose',[])
        value[name]['profile_packs']=sample.get('rf_scene_vehicle_profile_packs')
        value[name]['submarine_boarding']=sample.get('rf_scene_submarine_boarding')
        value[name]['submarine_weapon']=sample.get('rf_scene_submarine_weapon')
        if len(pose)==59:
            value[name]['actor_pose']=dict(radius=f(pose[1]),position=[f(v) for v in pose[14:17]],
                public_position=[f(v) for v in pose[17:20]],velocity=[f(v) for v in pose[23:26]],
                minimum=[f(v) for v in pose[53:56]],maximum=[f(v) for v in pose[56:59]])
    return value


def saved_rows(payload):
    data=component(payload,11)
    if len(data)<344 or data[:4]!=b'RFSW':raise RuntimeError('Missing outer switched-owner envelope')
    version,size,uid,profile,count=struct.unpack_from('<5I',data,4)
    if (version,uid,profile,count)!=(3,JEEP,3,1) or len(data)!=24+size+320:
        raise RuntimeError('Expected RFSW3 active Jeep and one parked submarine')
    bank=list(struct.unpack('<80I',data[24+size:]));inner=data[24:24+size]
    if len(inner)<96 or inner[:4]!=b'RFVA':raise RuntimeError('Missing passive submarine envelope')
    version,size,count=struct.unpack_from('<3I',inner,4)
    if (version,size,count)!=(2,160,1) or len(inner)!=16+size+80:raise RuntimeError('Expected RFVA2 and route-free Jeep160')
    vehicle=inner[16:16+size];passive=list(struct.unpack('<20I',inner[16+size:]))
    if vehicle[:4]!=b'RFVC' or struct.unpack_from('<2I',vehicle,4)!=(3,160) or struct.unpack_from('<2I',vehicle,16)!=(3,3) or struct.unpack_from('<I',vehicle,108)[0]!=1:
        raise RuntimeError('Active Jeep did not save occupied gunner state')
    v=list(struct.unpack('<40I',vehicle))
    if bank[:2]!=[SUB,1] or bank[15]!=4 or bank[3] or bank[8] or bank[79] or passive[:2]!=[SUB,0] or passive[19]:
        raise RuntimeError('Parked submarine lost profile, identity or living quiescent state')
    if bank[16:28]!=passive[2:14] or not math.isfinite(f(bank[40])) or f(bank[40])>0:
        raise RuntimeError('Parked submarine pose or settled torpedo cooldown changed')
    return dict(active_ammo=v[26],active_rng=v[30],parked_ammo=bank[4],parked_rng=0,
        active_position=v[8:11],parked_position=passive[2:5],active_health=v[6],
        parked_health=passive[14],active_armor=v[7],parked_armor=passive[15])


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0 or result['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB vehicle continuation')
    x=result['extra']
    if x['rf_scene_vehicle_state'][5] or any(x['rf_scene_npc_seats']) or any(x['rf_scene_setup_result']) or x['rf_scene_vehicle_route_state'][0] or x['rf_scene_vehicle_route_state'][3] or x['rf_scene_vehicle_route_state'][7] or x['rf_scene_combat'][0] or x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7]:
        raise RuntimeError('Unexpected owner, route, handheld fire or gameplay error')


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES);pre,x=source['probe'],source['extra'];state=source['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary transitioned Jeep save failed: {state}')
    if not 140<=pre['frame']<160 or pre['rf_scene_vehicle_enabled']!=[4] or pre['rf_scene_vehicle_state'][1:4]!=[1,0,1]:
        raise RuntimeError('Original submarine did not board normally before exchange')
    torpedo=pre['rf_scene_submarine_weapon']
    if torpedo[1]<1 or torpedo[7]>=20 or not pre['rf_scene_submarine_boarding'][4]:raise RuntimeError('Real wet submarine boarding/torpedo expenditure not established')
    if x['rf_scene_submarine_weapon'][4]:raise RuntimeError('Torpedo has not settled by ordinary save endpoint')
    sw=x['rf_scene_vehicle_switch'];a=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:6]!=[SUB,JEEP] or sw[8] or sw[15]!=JEEP or a[:4]!=[SUB,JEEP,pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12]] or a[8]!=torpedo[7] or a[29]!=1:
        raise RuntimeError(f'Ordinary submarine-to-Jeep transaction failed: {sw}')
    if x['rf_scene_vehicle_enabled']!=[3] or x['rf_scene_vehicle_state'][1:4]!=[2,1,1] or x['rf_scene_jeep_seats'][4]!=1 or x['rf_scene_apc_primary'][1]<1 or x['rf_scene_apc_primary'][7]>=a[9]:
        raise RuntimeError('Jeep did not board, select gunner and spend its own ammunition')
    saved=saved_rows(payload)
    if saved['active_ammo']!=x['rf_scene_apc_primary'][7] or saved['parked_ammo']!=torpedo[7] or saved['active_position']!=x['rf_scene_vehicle_state'][6:9] or saved['parked_position']!=a[12:15]:
        raise RuntimeError('Saved actual ammunition or owner poses changed')
    if [saved['parked_health'],saved['active_health'],saved['parked_armor'],saved['active_armor']]!=a[4:8]:raise RuntimeError('Saved vehicle vitals changed')
    return saved


def validate(source,loaded,payload,recipe):
    saved=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    pre,x=loaded['probe'],loaded['extra'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):raise RuntimeError('Ordinary profile4 bank load failed')
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_enabled']!=[3] or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][7]!=saved['active_ammo']:
        raise RuntimeError('Occupied Jeep did not restore without reboarding')
    restore=x['rf_scene_vehicle_switch_restore']
    expected=[1,JEEP,3,1,1,SUB,saved['parked_ammo'],0,0,0,0,saved['active_ammo'],saved['active_rng'],pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12],0]
    if restore!=expected or pre['rf_scene_vehicle_switch_restore']!=restore:raise RuntimeError(f'Submarine reserve/Jeep RNG or actual owners not restored: {restore}')
    sw=x['rf_scene_vehicle_switch'];a=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:6]!=[JEEP,SUB] or sw[8] or sw[9]!=1 or sw[15]!=SUB or sw[10:12]!=[saved['active_ammo'],saved['parked_ammo']]:raise RuntimeError(f'Ordinary return to saved submarine failed: {sw}')
    if x['rf_scene_vehicle_enabled']!=[4] or x['rf_scene_vehicle_state'][1:4]!=[1,1,1] or x['rf_scene_submarine_weapon'][7]!=saved['parked_ammo'] or not x['rf_scene_submarine_boarding'][4]:raise RuntimeError('Returned submarine lacks ordinary wet boarding and retained supply')
    if a[:4]!=[JEEP,SUB,pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12]] or a[15:18]!=saved['parked_position'] or a[24:30]!=[1,0,0,0,0,1] or a[31] or not 199<=a[30]<=201:raise RuntimeError('Return lost identity, saved pose or ordinary input timing')
    if a[4:8]!=[saved['active_health'],saved['parked_health'],saved['active_armor'],saved['parked_armor']]:raise RuntimeError('Return changed retained vehicle vitals')
    for sample in (pre,x):
        if sample['rf_scene_apc_primary'][1] or sample['rf_scene_submarine_weapon'][1]:raise RuntimeError('Fresh load replayed a weapon launch')
    return dict(result='PASS',saved=saved,restore=restore,return_exchange=a,
        wet_submarine_return=True,free_pages=loaded['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path);parser.add_argument('--resume-saved-run',type=Path)
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-sub-switch-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',phases={},recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L10S4.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve();source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes();report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,600,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=140,allow_guest_error=True)
            report['phases']['save']=source;report['diagnostics_save']=diagnostics(source)
            if source['guest_phase']!=5:
                raise RuntimeError(f"Source guest stopped phase {source['guest_phase']:08x}; checkpoint {source['checkpoint_state']}")
            payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=source;report['diagnostics_save']=diagnostics(source)
        report['saved']=validate_source(source,payload,recipe)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(True))
        build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,
            extra_symbols=SYMBOLS,probe=live_probe,probe_frame=40,allow_guest_error=True)
        report['phases']['load']=loaded;report['diagnostics_load']=diagnostics(loaded)
        report.update(validate(source,loaded,payload,recipe))
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
