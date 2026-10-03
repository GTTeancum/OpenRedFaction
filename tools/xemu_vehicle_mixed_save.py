"""One ordinary Xbox Jeep/APC save and fresh-boot return continuation.

Source reuses the passed mixed380 replay, saving the occupied APC after firing
has settled. Load probes at40, exits60, holds neutral and Uses100.
The return approach is an initial numerical schedule, not a claimed pass;
retained entry diagnostics guide any adjustment without forced poses or input
outside the guest. No images, campaign traversal or additional test matrix.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct

from xemu_vehicle_mixed_types import (JEEP,APC,FRAMES,SYMBOLS as MIXED_SYMBOLS,
    prepare_level as prepare_mixed,replay as mixed_replay,validate as validate_mixed,
    diagnostics)
from xemu_npc_jeep_detached_save import component
from build_fragment_platform_fixture import U
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES,LOAD_FRAMES=FRAMES,140
SYMBOLS=dict(MIXED_SYMBOLS,rf_scene_world_load_reject=3,
    rf_scene_checkpoint_world_reject=9,rf_scene_vehicle_restore_trace=8,
    rf_scene_vehicle_switch_restore=16)


def replay(load=False):
    if not load:return mixed_replay()
    # APC's mirrored local+X exit faces the inter-vehicle gap. Actual exit
    # clearance chooses the safe candidate; this does not overwrite its pose.
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',
        0,0,0,0,0,0,0,int(frame in (60,100)),0,0,0,0)
        for frame in range(LOAD_FRAMES))


def prepare_level(folder):
    path,recipe=prepare_mixed(folder);folder=Path(folder)
    recipe.update(scope=__doc__,runtime_status='PENDING_NATIVE_SAVE_CONTINUATION',
        frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
        load_replay={'probe_occupied_APC':40,'exit_APC':60,
                     'neutral_after_exit':[61,99],'return_Jeep':100},
        limitations='One occupied APC save/fresh-boot and parked Jeep return; initial return input schedule awaits native collision evidence. No moving handoff, secondary fire, other profiles or audiovisual claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    (folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    values={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    values['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return values


def saved_rows(payload):
    data=component(payload,11)
    if len(data)<344 or data[:4]!=b'RFSW':raise RuntimeError('Missing outer switched-owner envelope')
    version,size,uid,profile,count=struct.unpack_from('<5I',data,4)
    if (version,uid,profile,count)!=(2,APC,2,1) or len(data)!=24+size+320:
        raise RuntimeError('Expected RFSW2 active APC and one parked Jeep')
    inner=data[24:24+size];bank=data[24+size:]
    if len(inner)<96 or inner[:4]!=b'RFVA':raise RuntimeError('Missing ordinary passive-owner envelope')
    version,host_bytes,passive_count=struct.unpack_from('<3I',inner,4)
    if (version,host_bytes,passive_count)!=(2,128,1) or len(inner)!=16+host_bytes+80:
        raise RuntimeError('Expected route-free APC128 and one RFVA2 owner row')
    vehicle=inner[16:16+host_bytes];passive=inner[16+host_bytes:]
    if vehicle[:4]!=b'RFVC' or struct.unpack_from('<2I',vehicle,4)!=(2,128) or struct.unpack_from('<2I',vehicle,16)!=(2,3):
        raise RuntimeError('Active APC did not save its living occupied RFVC2 state')
    b=list(struct.unpack('<80I',bank));v=list(struct.unpack('<32I',vehicle))
    p=list(struct.unpack('<20I',passive))
    if b[:2]!=[JEEP,1] or b[3] or b[8] or b[15]!=3 or b[79]:
        raise RuntimeError('Parked RFSW2 owner lost Jeep profile or quiet unoccupied role')
    if p[:2]!=[JEEP,0] or p[19] or b[16:28]!=p[2:14]:
        raise RuntimeError('Parked living Jeep pose/identity differs between bank and RFVA')
    cooldown,warmup=struct.unpack_from('<2f',bank,160)
    secondary_cooldown=struct.unpack_from('<f',bank,176)[0]
    # The scheduler retains a finite negative cooldown after its final tick.
    # Match engine quiescence (not positive), rather than requiring zero bits.
    if any(not math.isfinite(value) or value>0 for value in (cooldown,warmup,secondary_cooldown)) or b[42] or v[31]:
        raise RuntimeError('Unexpected held fire/cooldown or nonzero RFVC reserved word')
    return dict(active_uid=uid,active_profile=profile,active_ammo=v[26],active_rng=v[30],
        active_secondary=v[27],active_position=v[8:11],active_health=v[6],active_armor=v[7],
        parked_uid=b[0],parked_profile=b[15],parked_ammo=b[4],parked_rng=b[6],
        parked_secondary=b[5],parked_position=p[2:5],parked_health=p[14],parked_armor=p[15])


def validate_source(source,payload,recipe):
    validate_mixed(source,recipe)
    state=source['checkpoint_state'];x=source['extra'];pre=source['probe']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary occupied APC save failed: {state}')
    saved=saved_rows(payload);first=x['rf_scene_vehicle_switch_apply']
    if saved['active_ammo']!=x['rf_scene_apc_primary'][7] or saved['parked_ammo']!=pre['rf_scene_apc_primary'][7]:
        raise RuntimeError('Saved active/parked ammunition differs from real fired owners')
    if saved['active_position']!=x['rf_scene_vehicle_state'][6:9] or saved['parked_position']!=first[12:15]:
        raise RuntimeError('Saved active/parked positions differ from actual owners')
    if [saved['parked_health'],saved['active_health'],saved['parked_armor'],saved['active_armor']]!=first[4:8]:
        raise RuntimeError('Save replaced distinct authored health or armor')
    return saved


def validate(source,loaded,payload,recipe):
    saved=validate_source(source,payload,recipe)
    if loaded['guest_phase']!=5 or loaded['frames']!=LOAD_FRAMES or loaded['memory_bytes']!=64*1024*1024 or loaded['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB fresh-load continuation')
    x=loaded['extra'];pre=loaded['probe'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError('Ordinary mixed-owner fresh load failed')
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_enabled']!=[2] or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_apc_primary'][7]!=saved['active_ammo']:
        raise RuntimeError('Saved APC did not restore occupied with its exact ammunition')
    if pre['rf_scene_apc_primary'][1] or pre['rf_scene_vehicle_switch'][1]:
        raise RuntimeError('Fresh load replayed firing or a Use transaction before the probe')
    restored=x['rf_scene_vehicle_switch_restore']
    expected=[1,APC,2,1,1,JEEP,saved['parked_ammo'],saved['parked_rng'],0,0,0,
        saved['active_ammo'],saved['active_rng'],pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12],0]
    if restored!=expected or pre['rf_scene_vehicle_switch_restore']!=restored:
        raise RuntimeError(f'Exact ammunition/RNG and real-owner restore failed: {restored}')
    sw=x['rf_scene_vehicle_switch'];apply=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:6]!=[APC,JEEP] or sw[8] or sw[9]!=1 or sw[15]!=JEEP or sw[10:12]!=[saved['active_ammo'],saved['parked_ammo']]:
        raise RuntimeError(f'Ordinary return did not restore parked Jeep ownership/ammunition: {sw}')
    if x['rf_scene_vehicle_enabled']!=[3] or x['rf_scene_vehicle_state'][1:4]!=[1,1,1] or x['rf_scene_apc_primary'][7]!=saved['parked_ammo'] or x['rf_scene_jeep_seats'][4]:
        raise RuntimeError('APC exit/Jeep driver boarding did not retain the saved profile and ammunition')
    if apply[:4]!=[APC,JEEP,pre['rf_scene_vehicle_state'][12],x['rf_scene_vehicle_state'][12]] or apply[8:12]!=[saved['active_ammo'],saved['parked_ammo'],saved['active_secondary'],saved['parked_secondary']]:
        raise RuntimeError('Return exchange lost stable registry identity or weapon banks')
    if apply[15:18]!=saved['parked_position'] or apply[24:30]!=[1,0,0,0,0,1] or apply[31] or not 99<=apply[30]<=101:
        raise RuntimeError('Return lost exact parked pose, quiet role or ordinary Use timing')
    if apply[4:8]!=[saved['active_health'],saved['parked_health'],saved['active_armor'],saved['parked_armor']]:
        raise RuntimeError('Return changed either owner health or armor')
    entry=x['rf_scene_vehicle_entry_probe']
    if not entry[0] or not 99<=entry[1]<=101 or entry[2:6]!=[3,x['rf_scene_vehicle_state'][12],1,0] or entry[14]!=2:
        raise RuntimeError(f'Ordinary Jeep range/path admission was not established: {entry}')
    for sample in (pre,x):
        if sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_damage'][3] or not sample['rf_scene_vehicle_damage'][7]:
            raise RuntimeError('Vehicle error/destruction during fresh-load continuation')
        if sample['rf_scene_apc_primary'][1] or sample['rf_scene_combat'][0] or any(sample['rf_scene_npc_seats']) or any(sample['rf_scene_setup_result']):
            raise RuntimeError('Unexpected shot, actor seat or setup replay after load')
        if sample['rf_scene_vehicle_route_state'][0] or sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_vehicle_route_state'][7] or sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7]:
            raise RuntimeError('Unexpected route or NPC activity after load')
    return dict(result='PASS',saved=saved,restore=restored,return_exchange=apply,
        no_reboard_of_restored_APC=True,exact_both_ammunition_and_rng=True,
        free_pages=loaded['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path);parser.add_argument('--resume-saved-run',type=Path)
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-mixed-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',phases={},recipe=recipe)
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
            report['phases']['save']=source;report['diagnostics_save']=diagnostics(source)
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
