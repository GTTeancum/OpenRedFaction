"""Bounded Xbox enemy combat while player occupies an authored Jeep gunner seat.

Real Jeep7629/miner7646 and pistol guard941 in CTF06. Ordinary Catatonic/Waiting
and Attack events stage combat after Use boarding. Real chassis damage must
occur before normal exit; no player fire, route, health injection or host input.
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
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows,f
from xemu_npc_turret_seat import record_details
from xemu_npc_jeep_gunner import prepare_level as prepare_pair, SYMBOLS as GUNNER_SYMBOLS
from xemu_npc_jeep_seat import ACTOR,HOST
from xemu_npc_jeep_detached_save import STOP
from xemu_npc_actor_interception import command
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SHOOTER=941
SETUP,START,PACIFY,WAIT,ATTACK,QUIET=914610,914611,914612,914613,914614,914615
FRAMES=240
SYMBOLS=dict(GUNNER_SYMBOLS,rf_scene_script_attack=12,rf_scene_combat=8,
             rf_scene_combat_event_count=1,rf_scene_combat_events=160,
             rf_scene_pickups=8,rf_scene_pickup_vitals=4)


def prepare_level(folder):
    path,recipe=prepare_pair(folder)
    raw=read_entry(path,'L12S1.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L12S1.rfl'))
    pair=entity_rows(raw)
    guard=next(r for r in entity_rows(read_entry(ROOT/'Installed_Game/levels1.vpp','L3S2.rfl')) if r['uid']==SHOOTER)
    details=record_details(guard)
    if guard['name']!='guard1' or details['seat_host_uid']!=-1 or b'12mm handgun' not in guard['raw']:
        raise RuntimeError('Expected original unseated pistol guard941')
    host=recipe['host_position']
    position=(host[0]+6,0,host[2])
    value=bytearray(guard['raw']);at=guard['transform']
    value[at:at+48]=F(*position,-1,0,0,0,0,1,0,1,0)
    if value[:at]!=guard['raw'][:at] or value[at+48:]!=guard['raw'][at+48:]:
        raise RuntimeError('Guard modified beyond transform')
    # Both actions use ordinary event propagation order. Waiting's authored
    # enum1 restores runtime action2 before the Attack request at frame120.
    events=U(7)+event(SETUP,'Delay','park_and_wait',(STOP,PACIFY))+\
        event(STOP,'Set_AI_Mode','park_jeep',(HOST,))+\
        command(PACIFY,'Set_AI_Mode','wait_for_mount',(SHOOTER,),0)+\
        command(START,'Delay','start_after_mount',(WAIT,ATTACK,QUIET),delay=1.0)+\
        command(WAIT,'Set_AI_Mode','resume_guard',(SHOOTER,),1)+\
        command(ATTACK,'Attack','attack_occupied_chassis',(HOST,),SHOOTER)+\
        command(QUIET,'Set_AI_Mode','quiet_before_exit',(SHOOTER,),0,delay=1.5)
    replacements={0x30000:U(3)+b''.join(r['raw'] for r in pair)+value,
                  0x600:events,0x40000:U(0)}
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=replacements.get(kind,raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L12S1.rfl'))
    checked=entity_rows(out)
    if [r['uid'] for r in checked]!=[HOST,ACTOR,SHOOTER] or checked[2]['raw']!=bytes(value):
        raise RuntimeError('Authored fixture records did not round trip')
    archive(path,[('L12S1.rfl',out)])
    recipe.update(scope=__doc__,frames=FRAMES,
                  setup_frames={str(SETUP):0,str(START):60},attack_frame=120,
                  replay={'board':90,'live_probe':200,'quiet_event':210,'exit':220},
                  guard=dict(uid=SHOOTER,position=position,details=details,
                             source='levels1.vpp/L3S2.rfl',sha256=hashlib.sha256(guard['raw']).hexdigest()),
                  staged_fields=['entity transforms','ordinary parking/Catatonic/Waiting/Attack event graph','removed pickups'],
                  initial_chassis_health=400,
                  limitations='One scripted handgun attack against an occupied Jeep; no autonomous acquisition, other vehicle types, item-collection gating, visual or audio verification.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def replay():
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,int(frame in (90,220)),0,0,0,0) for frame in range(FRAMES))


def live_probe(monitor,mapping):
    sample={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    sample['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return sample


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live,final=result['probe'],result['extra']
    if not 200<=live['frame']<220:
        raise RuntimeError('Missed live mounted-combat window')
    seat,pair=live['rf_scene_npc_seats'],live['rf_scene_npc_seat_probe']
    npc,host=pair[2:4]
    if seat[:4]!=[1,1,0,0] or seat[5:]!=[0,1,ACTOR,HOST,0] or pair[:2]!=[ACTOR,HOST] or pair[6:8]!=[13,host] or f(pair[4])!=1:
        raise RuntimeError(f'Living authored driver did not retain seat: {seat}, {pair}')
    gunner=live['rf_scene_jeep_npc_gunner'];player=gunner[9]
    # Third field counts blocked seat switches; this replay never requests one.
    if gunner[:3]!=[1,0,0] or gunner[4:9]!=[1,host,npc,npc,player] or gunner[10:16]!=[host,1,1,0,0,1] or player in (npc,host,0xffffffff):
        raise RuntimeError(f'Player was not seated separately during combat: {gunner}')
    if live['rf_scene_vehicle_state'][1:4]!=[1,0,1]:
        raise RuntimeError('No active mounted player vehicle session')
    attack=live['rf_scene_script_attack'];damage=live['rf_scene_vehicle_damage']
    attacker=attack[11]
    if attack[:5]!=[1,ATTACK,SHOOTER,host,1] or not attack[5] or attacker in (npc,host,player,0xffffffff):
        raise RuntimeError(f'Ordinary enemy Attack did not fire at real host: {attack}')
    # Field6 is last destroyer, unused on this living chassis; rows below prove shot source.
    if not 0<f(damage[0])<recipe['initial_chassis_health'] or not damage[2] or damage[3:6]!=[0,0,0] or damage[6]!=0xffffffff or damage[7]!=1:
        raise RuntimeError(f'Enemy failed to damage living chassis while player seated: {damage}')
    count=live['rf_scene_combat_event_count'][0]
    if not 1<=count<=32:raise RuntimeError(f'Invalid bounded combat-event count: {count}')
    rows=[live['rf_scene_combat_events'][i*5:i*5+5] for i in range(count)]
    if any(not 120<=row[0]<220 or row[1:3]!=[2,attacker] or not math.isfinite(f(row[3])) or f(row[3])<=0 for row in rows):
        raise RuntimeError(f'Unexpected source/timing or non-chassis damage: {rows}')
    end=final['rf_scene_jeep_npc_gunner']
    if end[:3]!=[1,1,0] or end[4:9]!=[0,host,npc,npc,0xffffffff] or end[9:16]!=[player,0xffffffff,0,1,0,0,1] or final['rf_scene_vehicle_state'][1:4]!=[1,1,0]:
        raise RuntimeError(f'Normal exit failed to preserve driver and release player: {end}')
    if final['rf_scene_npc_seats'][5:7]!=[1,0] or final['rf_scene_npc_seat_probe'][4]!=pair[4]:
        raise RuntimeError('Driver died or retained seat through normal teardown')
    for sample in (live,final):
        combat=sample['rf_scene_enemy_combat']
        if not combat[2] or not combat[3] or combat[7] or f(combat[5])!=100 or sample['rf_scene_combat'][0]:
            raise RuntimeError('Enemy combat inactive/failed or player handheld/self-damage occurred')
        if sample['rf_scene_apc_primary'][1] or sample['rf_scene_pickups'][3] or sample['rf_scene_pickups'][7] or any(sample['rf_scene_script_slays']):
            raise RuntimeError('Unexpected player vehicle fire, pickup or forced death')
        if sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_vehicle_route_state'][7] or sample['rf_scene_npc_seats'][9]:
            raise RuntimeError('Unexpected route motion or vehicle/seat error')
    if final['rf_scene_setup_result']!=[2,START,48,0]:
        raise RuntimeError('Ordinary event setup failed')
    return dict(result='PASS',mounted_enemy_shots=live['rf_scene_enemy_combat'][2],
                mounted_chassis_hits=damage[2],mounted_chassis_health=f(damage[0]),
                initial_chassis_health=recipe['initial_chassis_health'],source_handle=attacker,
                vehicle_handle=host,combat_events=rows,driver_health=f(pair[4]),normal_exit=True,
                free_pages=result['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-combat-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SETUP,START));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'combat')
        result=run_guest(folder,'combat',hdd,FRAMES,480,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=200,allow_guest_error=True)
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
