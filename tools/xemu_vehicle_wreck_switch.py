"""Bounded Xbox recovery from a destroyed occupied Jeep to a healthy Jeep.

Unchanged real two-Jeep CTF06 fixture. Ordinary Use boards A, a delayed
Slay_Object destroys it, collision-admitted ejection releases the player,
then ordinary Use boards B and its real gun fires. No forced damage state,
ownership, ammunition, host input, images, campaign route or save extension.
"""
import argparse
import datetime
import io
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_vehicle_switch import A,B,SYMBOLS as SWITCH_SYMBOLS,prepare_level as prepare_pair
from xemu_turret_combat import f
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SLAY=915210
FRAMES=350
SYMBOLS=dict(SWITCH_SYMBOLS,rf_scene_script_slays=6,rf_scene_passive_damage=8)


def replay():
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(frame in (90,250)),int(290<=frame<298),0,int(frame==270),0)
        for frame in range(FRAMES))


def prepare_level(folder):
    path,recipe=prepare_pair(folder);raw=read_entry(path,'ctf06.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    events=U(1)+command(SLAY,'Slay_Object','destroy_occupied_first_jeep',(A,),delay=2.0)
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=events if kind==0x600 else raw[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    archive(path,[('ctf06.rfl',out)])
    recipe.update(scope=__doc__,frames=FRAMES,setup_event=SLAY,
        replay={'board_A':90,'ordinary_slay_A':120,'probe_ejected_wreck':180,
                'board_B':250,'gunner_B':270,'fire_B':[290,297]},
        limitations='One parked occupied Jeep wreck and same-class recovery; no moving/crowded ejection, cross-profile switch, wreck save or audiovisual verification.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def live_probe(monitor,mapping):
    sample={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    sample['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return sample


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    pre,end=result['probe'],result['extra'];wreck=pre['rf_scene_vehicle_damage']
    if not 180<=pre['frame']<250 or pre['rf_scene_vehicle_selection']!=[2,2,0,0,0,0,0,2,A,3,2,0]:
        raise RuntimeError('Missing originally selected first Jeep wreck probe')
    if pre['rf_scene_vehicle_state'][1:4]!=[1,1,0] or f(wreck[0])>0 or wreck[2:6]!=[1,1,0,1] or wreck[7]:
        raise RuntimeError(f'Occupied first Jeep was not destroyed and safely released once: {wreck}')
    if pre['rf_scene_vehicle_switch'][1] or pre['rf_scene_apc_primary'][1]:
        raise RuntimeError('Unexpected handoff/fire before wreck recovery')
    slay=pre['rf_scene_script_slays']
    if slay[:3]!=[1,1,A] or slay[3]!=wreck[0] or not 1980<=slay[4]<=2020 or slay[5] or end['rf_scene_script_slays']!=slay:
        raise RuntimeError(f'Ordinary Slay did not destroy exactly the original host once: {slay}')
    sw=end['rf_scene_vehicle_switch'];applied=end['rf_scene_vehicle_switch_apply']
    old_handle=pre['rf_scene_vehicle_state'][12];new_handle=end['rf_scene_vehicle_state'][12]
    if old_handle in (0,0xffffffff) or new_handle in (0,0xffffffff,old_handle):
        raise RuntimeError('Recovery failed to retain distinct real owner handles')
    if sw[1]!=1 or sw[4:8]!=[A,B,old_handle,new_handle] or sw[8] or sw[9] or sw[15]!=B:
        raise RuntimeError(f'Ordinary Use did not switch from the abandoned wreck: {sw}')
    if applied[:4]!=[A,B,old_handle,new_handle] or applied[4]!=wreck[0] or f(applied[5])!=recipe['details']['authored_health'] or applied[24:30]!=[0,0,0,0,0,1] or not 249<=applied[30]<=251 or applied[31]!=1:
        raise RuntimeError(f'Exchange lost wreck state, living replacement or stable registry identity: {applied}')
    history=end['rf_scene_vehicle_switch_history']
    if history[:32]!=applied or any(history[32:]):raise RuntimeError('Unexpected additional owner exchange')
    damage=end['rf_scene_vehicle_damage']
    if f(damage[0])!=recipe['details']['authored_health'] or damage[2:6]!=[0,0,0,0] or damage[7]!=1:
        raise RuntimeError(f'Destroyed first owner contaminated healthy second vehicle: {damage}')
    if end['rf_scene_vehicle_state'][1:4]!=[2,1,1] or end['rf_scene_jeep_seats'][4]!=1 or end['rf_scene_apc_primary'][1]<1 or end['rf_scene_apc_primary'][7]>=applied[9]:
        raise RuntimeError('Recovered Jeep did not board, enter gunner role and spend real ammunition')
    for sample in (pre,end):
        if sample['rf_scene_setup_result']!=[1,SLAY,1,0] or sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_route_state'][0] or sample['rf_scene_vehicle_route_state'][3] or any(sample['rf_scene_npc_seats']):
            raise RuntimeError('Unexpected event failure, vehicle error, route or NPC occupancy')
        if sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_passive_damage'][2] or sample['rf_scene_passive_damage'][6]:
            raise RuntimeError('Unexpected secondary damage or NPC combat')
    return dict(result='PASS',wreck_before_handoff=wreck,exchange=applied,
                first_handle=old_handle,second_handle=new_handle,automatic_exits=1,
                healthy_shots=end['rf_scene_apc_primary'][1],
                remaining_ammo=end['rf_scene_apc_primary'][7],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-wreck-switch-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SLAY));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'wreck-switch');native=run_guest(folder,'wreck-switch',hdd,FRAMES,600,snapshot=True,
            extra_symbols=SYMBOLS,probe=live_probe,probe_frame=180,allow_guest_error=True)
        report['native']=native;report.update(validate(native,recipe))
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
