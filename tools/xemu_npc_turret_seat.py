"""Bounded Xbox authored NPC/turret seat check, without images or host input.

Copies complete L3S2 guard936 and turret935 records into CTF06, changing only
their transforms. Neutral replay exercises real seat binding, model-tag pose,
operator suppression and turret attack. No campaign routes or forced damage.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry, U, F
from inspect_levels import inspect
from xemu_turret_combat import entity_rows, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR=936
HOST=935
FRAMES=120
SYMBOLS={'rf_scene_npc_seats':10,'rf_scene_npc_seat_probe':12,
         'rf_scene_turret_operator':8,'rf_scene_turret_combat':10,
         'rf_scene_turret_shots':8,'rf_scene_enemy_combat':8,
         'rf_scene_turret_owners':8,'rf_scene_turret_draw':4,
         'rf_scene_turret_test':22,'rf_scene_player_vitals':6,
         'rf_scene_pickup_vitals':4}


def record_details(row):
    data=row['raw'];pos=row['transform']+48
    def skip_string():
        nonlocal pos
        size=struct.unpack_from('<H',data,pos)[0];pos+=2+size
        if pos>len(data):raise ValueError('Entity string outside record')
    skip_string();pos+=13;skip_string();skip_string()
    health,armor=struct.unpack_from('<2f',data,pos+17);pos+=29
    for _ in range(7):skip_string()
    seat=struct.unpack_from('<i',data,pos+6)[0]
    return dict(seat_host_uid=seat,authored_health=health,authored_armor=armor)


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L3S2.rfl')
    rows=entity_rows(source)
    host=next(r for r in rows if r['uid']==HOST)
    actor=next(r for r in rows if r['uid']==ACTOR)
    if host['name']!='Stationary Turret' or actor['name']!='guard1':
        raise ValueError('Authored seat pair classes changed')
    details=record_details(actor)
    if details['seat_host_uid']!=HOST:raise ValueError('Missing original seat-host UID')
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise ValueError('CTF06 already has entities')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',original,start['offset']+8)
    position=(spawn[0],spawn[1]+.530805886,spawn[2]-4)
    # Initial actor staging only. Runtime must republish the actual interface_1
    # model-tag pose and preserve its generation-checked physical host link.
    seat_local=(-.00165662507,-.55936294794,-.96528846025)
    actor_position=tuple(position[i]+seat_local[i] for i in range(3))
    copied=[]
    for row,point in ((host,position),(actor,actor_position)):
        raw=bytearray(row['raw']);at=row['transform']
        raw[at:at+48]=F(*point,0,0,1,1,0,0,0,1,0)
        if raw[:at]!=row['raw'][:at] or raw[at+48:]!=row['raw'][at+48:]:
            raise ValueError('Non-transform authored bytes changed')
        copied.append(raw)
    payload_entities=U(2)+b''.join(copied)
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0 and 0x30000 not in offsets:
            offsets[0x30000]=len(out);out+=U(0x30000,len(payload_entities))+payload_entities;added=1
        if kind==0x30000:payload=payload_entities
        if kind in (0x600,0x60000):payload=U(0)
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    check=entity_rows(out)
    if [r['uid'] for r in check]!=[HOST,ACTOR] or record_details(check[1])!=details:
        raise ValueError('Copied authored seat pair changed')
    size=4096+((len(out)+2047)&~2047);archive=bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(out))
    archive[4096:4096+len(out)]=out
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';path.write_bytes(archive)
    recipe=dict(source_archive='levels1.vpp',source_level='L3S2.rfl',spawn=spawn,
        host_position=position,actor_initial_position=actor_position,staged_fields=['transform'],
        actor_details=details,neutral_frames=FRAMES,
        records=[dict(uid=r['uid'],class_name=r['name'],source_offset=r['offset'],
                      bytes=len(r['raw']),sha256=hashlib.sha256(r['raw']).hexdigest()) for r in (host,actor)],
        scope='Real authored seat reference, binding, model-tag pose, operator suppression and turret fire; CTF06 pickups retained')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path


def live_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live=result['probe'];final=result['extra']
    seats=live['rf_scene_npc_seats'];operator=live['rf_scene_turret_operator']
    pose=live['rf_scene_npc_seat_probe'];last_pose=final['rf_scene_npc_seat_probe']
    if seats[0:4]!=[1,1,0,0] or seats[4]<2 or seats[5] or seats[6]!=1 or seats[7:10]!=[ACTOR,HOST,0]:
        raise RuntimeError(f'Live authored seat binding/pose failed: {seats}')
    if operator[0]!=1 or operator[1] or not operator[2] or not operator[4] or operator[6:]!=[ACTOR,0]:
        raise RuntimeError(f'Live operator eligibility/handheld suppression failed: {operator}')
    for state in (pose,last_pose):
        if state[0:2]!=[ACTOR,HOST] or state[2]==state[3] or 0xffffffff in state[2:4] or state[6]!=13 or state[7]!=state[3] or state[11]==0xffffffff:
            raise RuntimeError(f'Actor identity/action/physical link invalid: {state}')
        if not math.isfinite(f(state[4])) or f(state[4])<=0 or not all(math.isfinite(f(v)) for v in state[8:11]):
            raise RuntimeError(f'Invalid seated health/position: {state}')
    if pose[2:4]!=last_pose[2:4] or pose[4:6]!=last_pose[4:6]:
        raise RuntimeError('Seated operator identity/vitals changed during neutral attack')
    for extra in (live,final):
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7]:
            raise RuntimeError('Seated operator fired handheld weapon or enemy combat failed')
        if any(extra['rf_scene_turret_test']) or extra['rf_scene_npc_seats'][9] or extra['rf_scene_turret_operator'][7]:
            raise RuntimeError('Unexpected synthetic damage fixture or seating error')
    combat=final['rf_scene_turret_combat'];shots=final['rf_scene_turret_shots']
    if not combat[1] or not combat[3] or combat[7]!=HOST or combat[9] or shots[0]!=combat[3] or not shots[1] or shots[7]:
        raise RuntimeError(f'Occupied turret did not acquire/fire/damage: {combat}, {shots}')
    health_before=f(final['rf_scene_player_vitals'][0]);health_after=f(final['rf_scene_pickup_vitals'][0])
    if not math.isfinite(health_after) or health_after>=health_before or final['rf_scene_pickup_vitals'][2]:
        raise RuntimeError('Expected net player health loss without healing')
    if result['free_pages']<=0:raise RuntimeError('No free memory')
    return dict(result='PASS',actor_uid=ACTOR,host_uid=HOST,live_pose_updates=seats[4],
        live_operator_eligible=operator[2],live_handheld_suppressions=operator[4],
        actor_health=f(pose[4]),actor_armor=f(pose[5]),actor_position=[f(v) for v in pose[8:11]],
        actor_final_position=[f(v) for v in last_pose[8:11]],seat_tag=pose[11],
        shots=combat[3],damaging_hits=shots[1],handheld_shots=0,initial_player_health=health_before,
        final_player_health=health_after,player_death_observed=bool(result['player_life'][2]),
        armor_restored=f(final['rf_scene_pickup_vitals'][3]),free_pages=result['free_pages'],
        limitations='One staged authored pair; no save/load, dismount, animation appearance/audio or campaign route claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare copied assets only; no build/emulator')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-turret-seat-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':__doc__}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'seat')
        result=run_guest(folder,'seat',hdd,FRAMES,360,snapshot=True,extra_symbols=SYMBOLS,
                         allow_player_dead=True,probe=live_probe,probe_frame=30)
        report['native']=result;report.update(validate(result))
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
