"""Ordinary Xbox save/fresh-load of the authored guard936/turret935 seat pair.

Sixty neutral frames per boot, isolated CTF06, no campaign route or host input.
Checks RFNS ownership, RFTU target/cadence, restored live seating and real fire.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_npc_turret_seat import ACTOR, HOST, SYMBOLS as SEAT_SYMBOLS, prepare_level, f, U
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=60
SYMBOLS=dict(SEAT_SYMBOLS,rf_scene_npc_seat_checkpoint=6,rf_scene_turret_checkpoint=6,
             rf_scene_turret_restore_probe=8,rf_scene_world_load_reject=3,
             rf_scene_npc_checkpoint_reject_state=6)


def live_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def saved_rows(payload):
    if len(payload)<320 or payload[:4]!=b'RFWC':raise RuntimeError('Missing RFWC envelope')
    kind,offset,size=struct.unpack_from('<III',payload,128+10*12)
    if kind!=11 or offset<320 or offset+size>len(payload):raise RuntimeError('Invalid vehicle component')
    block=payload[offset:offset+size];rows={}
    # RFTU wraps RFNS, which wraps the unchanged vehicle payload. Accept either
    # wrapper order for transparent envelope parsing, but require one of each.
    for _ in range(2):
        if len(block)<16:raise RuntimeError('Missing seat/turret wrapper')
        magic=block[:4];version,inner,count=struct.unpack_from('<III',block,4)
        width={b'RFTU':184,b'RFNS':24}.get(magic)
        if not width or magic in rows or version!=1 or count!=1 or len(block)!=16+inner+width:
            raise RuntimeError('Invalid single-row seat/turret envelope')
        rows[magic]=block[16+inner:];block=block[16:16+inner]
    if block or set(rows)!={b'RFTU',b'RFNS'}:raise RuntimeError('Unexpected fixture vehicle payload')
    actor,host,tag,kind,active,owns_flag=struct.unpack('<6I',rows[b'RFNS'])
    if (actor,host,kind,active)!=(ACTOR,HOST,2,1) or tag==0xffffffff or owns_flag>1:
        raise RuntimeError('RFNS did not preserve active authored turret occupancy')
    row=rows[b'RFTU'];uid,cls=struct.unpack_from('<II',row)
    health,armor=struct.unpack_from('<ff',row,8)
    dead,flags,flags810,flags814,affiliation,action,target,remaining,burst=struct.unpack_from('<9I',row,16)
    if uid!=HOST or dead or not math.isfinite(health) or health<=0 or not math.isfinite(armor) or armor<0:
        raise RuntimeError('Invalid live RFTU owner')
    if not flags810&0x10000 or flags810&1 or target!=0 or remaining>36000:
        raise RuntimeError('RFTU lost occupied state or player target/cadence')
    combat_action=struct.unpack_from('<I',row,60)[0]
    return dict(seat=dict(actor_uid=actor,host_uid=host,tag=tag,kind=kind,active=active,owns_host_flag=owns_flag),
        turret=dict(uid=uid,class_index=cls,health=health,armor=armor,action=action,
                    target_uid=target,remaining=remaining,burst=burst,flags810=flags810,combat_action=combat_action))


def check_live(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    live=result['probe'];last=result['extra'];seats=live['rf_scene_npc_seats'];op=live['rf_scene_turret_operator']
    if seats[:4]!=[1,1,0,0] or seats[4]<2 or seats[6]!=1 or seats[7:10]!=[ACTOR,HOST,0]:
        raise RuntimeError(f'Missing unique live seat: {seats}')
    # Restore assigns a validated association instead of replaying bind. The
    # initial authored bind counter remains exactly one, never incremented twice.
    if op[0]!=1 or not op[2] or not op[4] or op[6:]!=[ACTOR,0]:
        raise RuntimeError(f'Operator missing/duplicated/not suppressing handheld: {op}')
    for extra in (live,last):
        pose=extra['rf_scene_npc_seat_probe']
        if pose[:2]!=[ACTOR,HOST] or pose[2]==pose[3] or 0xffffffff in pose[2:4] or pose[6]!=13 or pose[7]!=pose[3]:
            raise RuntimeError(f'Seated actor identity/action/host link invalid: {pose}')
        if not math.isfinite(f(pose[4])) or f(pose[4])<=0 or not all(math.isfinite(f(v)) for v in pose[8:11]):
            raise RuntimeError('Invalid seated actor vitals/pose')
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7] or any(extra['rf_scene_turret_test']):
            raise RuntimeError('Handheld shot/error or synthetic turret fixture detected')
        if extra['rf_scene_npc_seats'][9] or extra['rf_scene_turret_operator'][7] or extra['rf_scene_turret_combat'][9] or extra['rf_scene_turret_shots'][7]:
            raise RuntimeError('Seat/operator/turret gameplay error')
    return live['rf_scene_npc_seat_probe']


def validate_source(saved,payload):
    pose=check_live(saved);rows=saved_rows(payload);x=saved['extra'];state=saved['checkpoint_state']
    if state[9]!=1 or state[3] or x['rf_scene_npc_seat_checkpoint'][0]!=1 or x['rf_scene_npc_seat_checkpoint'][5] or x['rf_scene_turret_checkpoint']!=[1,0,0,0,0,0]:
        raise RuntimeError(f'Ordinary seated source save failed: {state}, {x}')
    if pose[11]!=rows['seat']['tag']:raise RuntimeError('Serialized seat tag differs from live tag')
    return rows


def validate(saved,loaded,payload):
    rows=validate_source(saved,payload);pose=check_live(loaded)
    x=loaded['extra'];state=loaded['checkpoint_state'];seat=x['rf_scene_npc_seat_checkpoint'];turret=x['rf_scene_turret_checkpoint']
    if state[8]!=1 or state[0] or state[1]!=saved['checkpoint_state'][4] or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary world restore failed: {state}')
    if seat[:4]!=[0,1,1,1] or seat[4]<1 or seat[5] or turret!=[0,1,1,0,1,0]:
        raise RuntimeError(f'Seat/turret restore missing/duplicated: {seat}, {turret}')
    expected=rows['turret'];restored=x['rf_scene_turret_restore_probe']
    due=restored[5]+expected['remaining'] if expected['remaining'] else 0
    if restored[:2]!=[HOST,0] or restored[2]==0xffffffff or restored[3]!=expected['remaining'] or restored[4]!=due or restored[6:]!=[expected['burst'],expected['combat_action']]:
        raise RuntimeError(f'Turret target/cadence did not restore exactly: {restored}, {expected}')
    source_pose=saved['extra']['rf_scene_npc_seat_probe']
    if pose[4:6]!=source_pose[4:6] or pose[11]!=rows['seat']['tag']:
        raise RuntimeError('Seated NPC vitals/tag changed across save/load')
    combat=x['rf_scene_turret_combat'];shots=x['rf_scene_turret_shots']
    if not combat[3] or combat[7]!=HOST or combat[8]!=restored[2] or shots[0]!=combat[3] or not shots[1] or shots[6]!=restored[2]:
        raise RuntimeError(f'Restored occupied turret did not resume damaging player attacks: {combat}, {shots}')
    before=f(saved['extra']['rf_scene_pickup_vitals'][0]);after=f(x['rf_scene_pickup_vitals'][0])
    if not math.isfinite(after) or after>=before or x['rf_scene_pickup_vitals'][2]:
        raise RuntimeError('Loaded real player health did not decrease without healing')
    return dict(result='PASS',saved=rows,restored_cadence=restored,
        restored_actor_health=f(pose[4]),restored_actor_armor=f(pose[5]),
        restored_actor_position=[f(v) for v in pose[8:11]],
        loaded_shots=combat[3],loaded_damaging_hits=shots[1],handheld_shots=0,
        player_health_at_save=before,player_health_after_load=after,
        player_death_observed=bool(loaded['player_life'][2]),
        free_pages=min(saved['free_pages'],loaded['free_pages']),
        limitations='One active authored turret seat and ordinary save/fresh-load; no Jeep, detached-seat, visual animation or campaign route claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing',type=Path,help='Read existing evidence only; no build/emulator')
    args=parser.parse_args()
    if args.validate_existing:
        folder=args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):raise RuntimeError('Disc restoration not confirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()),json.loads((folder/'load/result.json').read_text()),(folder/'save/xbox-world.rfwc').read_bytes()),indent=2));return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-turret-seat-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',scope=__doc__,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,FRAMES,420,capture_world=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=20)
        report['phases']['save']=saved;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved']=validate_source(saved,payload)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=20,allow_player_dead=True)
        report['phases']['load']=loaded;report.update(validate(saved,loaded,payload))
    except Exception as exc:
        report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
