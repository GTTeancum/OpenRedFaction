"""Bounded Xbox generated Auto Turret head, using one real authored base.

Copies L3S2 base1994 into CTF06, changing only its transform. No authored
Head record, forced readiness/damage, host input, images or campaign route.
Requires generated-head runtime telemetry described in AUTO-TURRET-CHECK.md.
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
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

BASE=1994
FRAMES=120
SYMBOLS={'rf_scene_turret_generated_stats':8,'rf_scene_turret_generated_probe':32,
         'rf_scene_turret_combat':10,'rf_scene_turret_shots':8,
         'rf_scene_turret_owners':8,'rf_scene_turret_draw':4,
         'rf_scene_enemy_combat':8,'rf_scene_turret_test':22,
         'rf_scene_player_vitals':6,'rf_scene_pickup_vitals':4}


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L3S2.rfl')
    owner=next(r for r in entity_rows(source) if r['uid']==BASE)
    if owner['name']!='Auto Turret' or record_details(owner)['seat_host_uid']!=-1:
        raise ValueError('Unexpected authored base class/attachment')
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise ValueError('CTF06 already has actors')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    spawn_section=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',original,spawn_section['offset']+8)
    # Staged at the CTF06 settled floor origin measured in the previous seat
    # fixture's RFPL. Side offset makes independent head yaw observable.
    position=(spawn[0]+.8,-.4120500683784485,spawn[2]-6)
    raw=bytearray(owner['raw']);at=owner['transform']
    raw[at:at+48]=F(*position,0,0,1,1,0,0,0,1,0)
    if raw[:at]!=owner['raw'][:at] or raw[at+48:]!=owner['raw'][at+48:]:
        raise ValueError('Changed non-transform authored bytes')
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    for section in meta['sections']:
        kind=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0 and 0x30000 not in offsets:
            entities=U(1)+raw;offsets[0x30000]=len(out);out+=U(0x30000,len(entities))+entities;added=1
        if kind==0x30000:payload=U(1)+raw
        if kind in (0x600,0x60000):payload=U(0)
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    check=entity_rows(out)
    if len(check)!=1 or check[0]['uid']!=BASE or check[0]['name']!='Auto Turret':
        raise ValueError('Fixture must contain one authored base and no Head record')
    size=4096+((len(out)+2047)&~2047);archive=bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(out));archive[4096:4096+len(out)]=out
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';path.write_bytes(archive)
    recipe=dict(source_archive='levels1.vpp',source_level='L3S2.rfl',base_uid=BASE,
        class_name=owner['name'],source_offset=owner['offset'],record_bytes=len(raw),
        source_record_sha256=hashlib.sha256(owner['raw']).hexdigest(),spawn=spawn,position=position,
        authored=record_details(owner),staged_fields=['transform'],authored_records=1,authored_heads=0,
        neutral_frames=FRAMES,scope='Generated owner, evaluated skeletal attachment, independent aim and real damage; pickups retained')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');return path


def live_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB generated-head run')
    live=result['probe'];x=result['extra'];stats=x['rf_scene_turret_generated_stats']
    if stats[0]!=1 or stats[1]<2 or not stats[3] or not stats[4] or any(stats[5:]):
        raise RuntimeError(f'Generated owner/publication/independent aim failed: {stats}')
    for extra in (live,x):
        p=extra['rf_scene_turret_generated_probe']
        if p[:2]!=[BASE,1] or p[2]==p[3] or 0xffffffff in p[2:4] or p[4:6]!=[0xffffffff,0xffffffff] or p[6]!=p[2]:
            raise RuntimeError(f'Invalid generated key/registration/base link: {p}')
        if f(p[7])!=80 or not math.isfinite(f(p[8])) or f(p[8])<=0 or not p[9]&1 or not p[10]&0x100 or p[11]==0xffffffff or p[30:]!=[1,0]:
            raise RuntimeError(f'Generated owner readiness/vitals/live count invalid: {p}')
        head=[f(v) for v in p[12:15]];tag=[f(v) for v in p[15:18]]
        if not all(math.isfinite(v) for v in head+tag) or max(abs(a-b) for a,b in zip(head,tag))>1e-5:
            raise RuntimeError(f'Head does not follow evaluated skeletal interface: {head}, {tag}')
        if not all(math.isfinite(f(v)) for v in p[18:30]):raise RuntimeError('Invalid head basis/base position')
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7] or any(extra['rf_scene_turret_test']):
            raise RuntimeError('Base fired handheld weapon or synthetic fixture active')
    p=x['rf_scene_turret_generated_probe'];combat=x['rf_scene_turret_combat'];shots=x['rf_scene_turret_shots']
    if not combat[1] or not combat[2] or not combat[3] or combat[9] or shots[0]!=combat[3] or not shots[1] or shots[7] or shots[6]!=combat[8]:
        raise RuntimeError(f'Generated head did not acquire/aim/fire/damage real target: {combat}, {shots}')
    if x['rf_scene_turret_owners'][0]!=1 or x['rf_scene_turret_owners'][7] or not x['rf_scene_turret_draw'][0] or x['rf_scene_turret_draw'][2]:
        raise RuntimeError('Generated owner/model was not retained and submitted')
    before=f(x['rf_scene_player_vitals'][0]);after=f(x['rf_scene_pickup_vitals'][0])
    if not math.isfinite(after) or after>=before or x['rf_scene_pickup_vitals'][2]:raise RuntimeError('Real player health did not decrease without healing')
    return dict(result='PASS',base_uid=BASE,role=1,head_uid=p[4],created_heads=stats[0],
        attachment_publications=stats[1],attachment_position_changes=stats[2],independent_aim_publications=stats[3],
        head_health=f(p[8]),head_position=[f(v) for v in p[12:15]],shots=shots[0],damaging_hits=shots[1],
        handheld_shots=0,initial_player_health=before,final_player_health=after,
        player_death_observed=bool(result['player_life'][2]),free_pages=result['free_pages'],
        limitations='Live generated attachment and combat only; motion count reported, no forced base animation, save/load, coupled death or visual/audio claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare copied assets; no build/emulator')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'auto')
        result=run_guest(folder,'auto',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
                         allow_player_dead=True,probe=live_probe,probe_frame=30)
        report['native']=result;report.update(validate(result))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
