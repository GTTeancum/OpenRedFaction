"""Bounded Xbox living authored Jeep-driver release after host destruction.

Grounded original Jeep7629/miner7646 in CTF06. Ordinary Set_AI_Mode parks the
Jeep; delayed Slay_Object destroys only its host. Real exit collision/placement,
seat release and subsequent NPC physics run without blast or health injection.
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
from xemu_npc_jeep_gunner import prepare_level as prepare_pair
from xemu_npc_jeep_seat import ACTOR,HOST,SYMBOLS as SEAT_SYMBOLS,f
from xemu_npc_jeep_detached_save import STOP
from xemu_npc_actor_interception import command
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SLAY=914900
FRAMES=180
SYMBOLS=dict(SEAT_SYMBOLS,rf_scene_vehicle_wreck_exit=12,
             rf_scene_vehicle_wreck_exit_apply=32,rf_scene_npc_jeep_detached_probe=16,
             rf_scene_combat=8,rf_scene_live_death_audio=4)


def prepare_level(folder):
    path,recipe=prepare_pair(folder)
    raw=read_entry(path,'L12S1.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L12S1.rfl'))
    events=U(2)+event(STOP,'Set_AI_Mode','park_before_host_death',(HOST,))+\
        command(SLAY,'Slay_Object','destroy_occupied_jeep',(HOST,),delay=1.0)
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=events if kind==0x600 else raw[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L12S1.rfl'))
    archive(path,[('L12S1.rfl',out)])
    recipe.update(scope=__doc__,frames=FRAMES,setup_frames={str(STOP):0,str(SLAY):60},
                  destruction_frame=120,pre_probe_frame=100,replay='neutral',
                  route_activated=False,
                  staged_fields=['authored entity transforms','ordinary park/delayed host Slay events'],
                  limitations='Parked Jeep only; transferred point velocity may be zero. No moving-host momentum, crowded-exit recovery, other vehicles, player ejection, saves or audiovisual-output claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def replay():return b'RFI6'+U(48)+bytes(FRAMES*48)


def live_probe(monitor,mapping):
    sample={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    sample['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return sample


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    before,end=result['probe'],result['extra']
    seats,pair=before['rf_scene_npc_seats'],before['rf_scene_npc_seat_probe']
    npc,host=pair[2:4]
    if not 100<=before['frame']<120 or seats[:4]!=[1,1,0,0] or seats[5:]!=[0,1,ACTOR,HOST,0] or pair[:2]!=[ACTOR,HOST] or pair[6:8]!=[13,host] or f(pair[4])!=1:
        raise RuntimeError(f'Original living driver/host seat absent before destruction: {seats}, {pair}')
    if any(before['rf_scene_vehicle_wreck_exit']) or any(before['rf_scene_vehicle_wreck_exit_apply']) or any(before['rf_scene_script_slays']):
        raise RuntimeError('Wreck handling ran before delayed ordinary destruction')
    initial=before['rf_scene_vehicle_damage']
    if f(initial[0])!=400 or initial[2:6]!=[0,0,0,0] or initial[7]!=1:
        raise RuntimeError(f'Expected undamaged authored Jeep before Slay: {initial}')
    slay=end['rf_scene_script_slays'];damage=end['rf_scene_vehicle_damage']
    if slay[:3]!=[1,1,HOST] or f(slay[3])>0 or not 1980<=slay[4]<=2020 or slay[5]:
        raise RuntimeError(f'Ordinary host Slay failed: {slay}')
    if f(damage[0])>0 or damage[2]!=1 or damage[3]!=1 or damage[7]:
        raise RuntimeError(f'Host did not become a real wreck through one damage request: {damage}')
    stats,apply=end['rf_scene_vehicle_wreck_exit'],end['rf_scene_vehicle_wreck_exit_apply']
    if stats[1]!=1 or stats[0]!=stats[2]+1 or stats[3] or stats[4]!=apply[0] or stats[5:7]!=[npc,host] or stats[7]>=8 or stats[8] or stats[10:]!=[ACTOR,HOST]:
        raise RuntimeError(f'Wreck exit missing, repeated or invalid: {stats}')
    if not 120<=apply[0]<FRAMES-15 or apply[1:7]!=[npc,host,0xffffffff,0xffffffff,0xffffffff,2] or apply[7]!=pair[4] or apply[26]!=stats[7] or apply[27]:
        raise RuntimeError(f'Exit failed to release live ownership: {apply}')
    floats=[f(v) for v in apply[8:26]]
    if not all(math.isfinite(v) for v in floats):raise RuntimeError('Nonfinite exit/host motion')
    seat,exit_,velocity,linear,angular,origin=[floats[i:i+3] for i in range(0,18,3)]
    offset=[seat[i]-origin[i] for i in range(3)]
    expected=[linear[0]+angular[1]*offset[2]-angular[2]*offset[1],
              linear[1]+angular[2]*offset[0]-angular[0]*offset[2],
              linear[2]+angular[0]*offset[1]-angular[1]*offset[0]]
    if max(abs(velocity[i]-expected[i]) for i in range(3))>.00002:
        raise RuntimeError(f'Exit velocity is not the host seat-point velocity: {velocity}, expected {expected}')
    # Actual-shape clearance is admitted by the runtime. The sampled pose
    # must additionally demonstrate a material displacement out of the seat.
    if math.dist(exit_,seat)<1 or math.hypot(exit_[0]-origin[0],exit_[2]-origin[2])<2:
        raise RuntimeError('Success telemetry did not actually move the living NPC outside the chassis origin')
    detached=end['rf_scene_npc_jeep_detached_probe'];final_position=[f(v) for v in detached[12:15]]
    if end['rf_scene_npc_seats'][5:]!=[1,0,ACTOR,HOST,0] or detached[:4]!=[ACTOR,HOST,npc,host] or detached[4]!=pair[4] or detached[5:10]!=[2,0xffffffff,0xffffffff,0xffffffff,0] or detached[15]!=1:
        raise RuntimeError(f'Living miner snapped back, died or retained ownership: {detached}')
    if not all(math.isfinite(v) for v in final_position) or math.dist(final_position,exit_)>.5:
        raise RuntimeError(f'Ordinary post-release physics lost the grounded parked exit: {final_position}, {exit_}')
    for sample in (before,end):
        if sample['rf_scene_vehicle_state'][3] or sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_vehicle_route_state'][7] or sample['rf_scene_npc_seats'][9]:
            raise RuntimeError('Unexpected player possession, route motion or vehicle/seat failure')
        if sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_combat'][0] or any(sample['rf_scene_live_death_audio']):
            raise RuntimeError('Unexpected NPC death/fire or player shots')
    if end['rf_scene_setup_result']!=[2,SLAY,1,0]:raise RuntimeError('Ordinary delayed host Slay request failed')
    return dict(result='PASS',actor_uid=ACTOR,host_uid=HOST,exit_frame=apply[0],
                exit_position=exit_,final_position=final_position,seat_position=seat,
                inherited_velocity=velocity,expected_point_velocity=expected,
                exits=stats[1],blocked_attempts=stats[2],miner_health=f(pair[4]),
                free_pages=result['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-jeep-wreck-exit-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,SLAY));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'wreck')
        result=run_guest(folder,'wreck',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=100,allow_guest_error=True)
        report['native']=result;report.update(validate(result,recipe))
    except Exception as error:report['error']=str(error);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as error:report['result']='FAIL';report['restore_error']=str(error);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
