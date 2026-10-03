"""Bounded Xbox ordinary scripted ignition with no fabricated weapon hit.

Original L20S1 comp_tech12020 and Ignite_Entity12694 in empty CTF06. Only
the actor transform changes; vitals, inventory, flags, event and links remain.
Activate the same ordinary event at0/60, then observe180 neutral frames.
No forced damage, immunity edits, original-game runtime, images or host input.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import pefile

from build_fragment_platform_fixture import read_entry,U,F
from check_ai_projectile_ordinary import archive
from inspect_events import inspect as inspect_events
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR,IGNITE,FRAMES=12020,12694,180
SYMBOLS={'rf_scene_script_ignite':8,'rf_scene_script_ignite_probe':12,
         'rf_scene_burning':5,'rf_scene_setup_result':4,
         'rf_scene_enemy_combat':8,'rf_scene_combat':8,
         'rf_scene_script_slays':6}


def original_records():
    source=read_entry(ROOT/'Installed_Game/levels2.vpp','L20S1.rfl')
    actor=next(r for r in entity_rows(source) if r['uid']==ACTOR)
    details=record_details(actor)
    if actor['name']!='comp_tech' or details!={'seat_host_uid':-1,'authored_health':100.0,'authored_armor':1.0}:
        raise ValueError('Expected original unseated L20S1 technician')
    meta=inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='L20S1.rfl'))
    section=next(s for s in meta['sections'] if s['type']=='0x600')
    data=source[section['offset']+8:section['offset']+8+section['size']]
    pe=pefile.PE(str(ROOT/'Installed_Game/RF.exe'))
    image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase;types=[]
    for pointer in range(0x5a1a3c,0x5a1ba4,4):
        at=struct.unpack_from('<I',image,pointer-base)[0]-base
        types.append(image[at:image.index(b'\0',at)].decode('cp1252'))
    row=next(r for r in inspect_events(data,types) if r['uid']==IGNITE)
    if row['type_index']!=82 or row['links']!=[ACTOR,12705] or row['delay']!=0:
        raise ValueError('Original ignition event differs')
    return actor,details,row,data[row['offset']:row['offset']+row['bytes']]


def prepare_level(folder):
    actor,details,authored,ignite=original_records()
    raw=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(raw):raise RuntimeError('Expected empty CTF06 actor testbed')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',raw,start['offset']+8)
    position=(spawn[0]+1.5,spawn[1],spawn[2]-3.5)
    placed=bytearray(actor['raw']);at=actor['transform']
    placed[at:at+48]=F(*position,1,0,0,0,0,-1,0,1,0)
    if placed[:at]!=actor['raw'][:at] or placed[at+48:]!=actor['raw'][at+48:]:
        raise ValueError('Actor changed beyond transform')
    replacements={0x30000:U(1)+placed,0x600:U(1)+ignite,0x60000:U(0),0x40000:U(0)}
    present={int(s['type'],16) for s in meta['sections']}
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={};added=0
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0:
            for missing in (0x30000,0x600):
                if missing not in present:
                    value=replacements[missing];offsets[missing]=len(out)
                    out+=U(missing,len(value))+value;added+=1
        value=replacements.get(kind,raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(value))+value
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=[bytes(placed)]:
        raise ValueError('Staged authored actor failed round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp'
    archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,actor_uid=ACTOR,source_archive='levels2.vpp',source_level='L20S1.rfl',
                source_details=details,position=position,changed_fields=['actor transform'],
                actor_sha256=hashlib.sha256(actor['raw']).hexdigest(),
                event_sha256=hashlib.sha256(ignite).hexdigest(),authored_event=authored,
                setup_frames=[0,60],frames=FRAMES,
                absent_original_link=12705,
                assumptions='Original technician remains mortal and unarmed. No replacement of authored flags or immediate damage injection.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path,recipe


def real(word):return struct.unpack('<f',struct.pack('<I',word))[0]


def validate(run,recipe):
    if run['guest_phase']!=5 or run['frames']!=FRAMES or run['memory_bytes']!=64*1024*1024 or run['free_pages']<=0 or run['player_life'][2]:
        raise RuntimeError('Incomplete bounded stock64MiB ignition run')
    x=run['extra'];ignition=x['rf_scene_script_ignite'];p=x['rf_scene_script_ignite_probe'];burn=x['rf_scene_burning']
    if ignition[:5]!=[2,1,1,0,ACTOR] or ignition[7]:
        raise RuntimeError(f'Ordinary repeated ignition did not remain idempotent: {ignition}')
    if p[:5]!=[1,ACTOR,ignition[5],ignition[6],0xffffffff] or not p[3]:
        raise RuntimeError(f'Unowned burn identity mismatch: {p}')
    initial=recipe['source_details']['authored_health'];before,after,current=map(real,p[5:8])
    if before!=initial or after!=before:
        raise RuntimeError(f'Ignition fabricated an immediate hit: {before}, {after}, expected{initial}')
    if not math.isfinite(current) or not 0<current<after:
        raise RuntimeError(f'Periodic burn failed to damage the living technician: {current}')
    ticks=300-p[8]
    if not FRAMES-2<=ticks<=FRAMES or p[9]!=ticks%15 or not FRAMES-2<=p[11]<=FRAMES:
        raise RuntimeError(f'Burn lifetime reset or phase/sample drift: {p}')
    # Closing the scene releases this one live burn after the last sample.
    if burn!=[1,ticks//15,1,0,0]:
        raise RuntimeError(f'Common quarter-second burn cadence/teardown mismatch: {burn}, ticks{ticks}')
    if x['rf_scene_setup_result']!=[2,IGNITE,82,0]:
        raise RuntimeError('Original event dispatch failed')
    combat=x['rf_scene_enemy_combat']
    if combat[2] or combat[7] or x['rf_scene_combat'][0] or any(x['rf_scene_script_slays']):
        raise RuntimeError('Weapon fire, Slay or enemy error contaminated ignition')
    return dict(result='PASS',created=ignition[1],repeat_ignored=ignition[2],
                initial_health=before,health_after_creation=after,health_after_burning=current,
                burn_pulses=burn[1],burn_ticks=ticks,remaining_ticks=p[8],
                limitations='One ordinary living technician; mortality over the full five-second lifetime, extinguishing, burning save/load and presentation are outside this check.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('scripted-ignite-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(IGNITE,IGNITE))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'ignite');report['native']=run_guest(folder,'ignite',hdd,FRAMES,360,snapshot=True,
            extra_symbols=SYMBOLS,allow_guest_error=True)
        report.update(validate(report['native'],recipe))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_build_error']=str(exc);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
