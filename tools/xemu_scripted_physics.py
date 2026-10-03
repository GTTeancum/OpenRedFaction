"""Bounded Xbox original linked NPC physics OFF and ordinary OFF-action wake.

L11S3 Turn_Off_Physics10651 targets the real hidden eos10636/miner10637.
In empty CTF06, ordinary UnHide exposes their staged airborne transforms,
then the original ON freezes physics. Invert at60 wakes ordinary gravity.
No velocity/health injection, route, images, host input or original runtime.
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
from check_hit_event import event
from inspect_levels import inspect
from inspect_events import inspect as inspect_events
from xemu_turret_combat import entity_rows,f
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTORS=(10636,10637)
PHYSICS=10651
SETUP,UNHIDE,WAKE=915000,915001,915002
POSITIONS=((4,1,-3),(-4,1,-3))
FRAMES=120
SYMBOLS={'rf_scene_physics_state':8,'rf_scene_physics_npcs':32,
         'rf_scene_enemy_combat':8,'rf_scene_combat':8,
         'rf_scene_script_slays':6,'rf_scene_setup_result':4}


def originals():
    source=read_entry(ROOT/'Installed_Game/levels2.vpp','L11S3.rfl')
    rows=entity_rows(source)
    actors=[next(r for r in rows if r['uid']==uid) for uid in ACTORS]
    if [r['name'] for r in actors]!=['eos','miner1'] or any(record_details(r)['seat_host_uid']!=-1 for r in actors):
        raise RuntimeError('Original unseated NPC pair differs')
    meta=inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='L11S3.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x600')
    data=source[sec['offset']+8:sec['offset']+8+sec['size']]
    pe=pefile.PE(str(ROOT/'Installed_Game/RF.exe'));image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    types=[]
    for address_ in range(0x5a1a3c,0x5a1ba4,4):
        at=struct.unpack_from('<I',image,address_-base)[0]-base
        types.append(image[at:image.index(b'\0',at)].decode('cp1252'))
    record=next(r for r in inspect_events(data,types) if r['uid']==PHYSICS)
    if record['type_index']!=62 or record['links']!=list(ACTORS) or record['delay']!=0 or record['header_byte']!=1:
        raise RuntimeError('Original linked Turn_Off_Physics differs')
    return actors,record,data[record['offset']:record['offset']+record['bytes']]


def prepare_level(folder):
    actors,authored,physics=originals()
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('Expected actor-free CTF06')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    copied=[]
    for row,position in zip(actors,POSITIONS):
        value=bytearray(row['raw']);at=row['transform']
        value[at:at+48]=F(*position,0,0,1,1,0,0,0,1,0)
        if value[:at]!=row['raw'][:at] or value[at+48:]!=row['raw'][at+48:]:
            raise RuntimeError('Original NPC modified beyond transform')
        copied.append(value)
    events=U(4)+event(SETUP,'Delay','freeze_setup',(UNHIDE,PHYSICS))+\
        event(UNHIDE,'UnHide','show_original_cutscene_actors',ACTORS)+physics+\
        event(WAKE,'Invert','wake_frozen_actors',(PHYSICS,))
    replacements={0x30000:U(2)+b''.join(copied),0x600:events,0x60000:U(0),0x40000:U(0)}
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    present={int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0:
            for missing in sorted(replacements.keys()-present):
                payload=replacements[missing];offsets[missing]=len(out);out+=U(missing,len(payload))+payload;added+=1
        payload=replacements.get(kind,original[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000]);struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=[bytes(r) for r in copied]:
        raise RuntimeError('NPC fixture failed exact round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,frames=FRAMES,source='levels2.vpp/L11S3.rfl',authored_event=authored,
                event_sha256=hashlib.sha256(physics).hexdigest(),
                actors=[dict(uid=r['uid'],class_name=r['name'],position=p,details=record_details(r),source_sha256=hashlib.sha256(r['raw']).hexdigest()) for r,p in zip(actors,POSITIONS)],
                staged_fields=['NPC transforms','ordinary UnHide/Invert setup'],
                setup_frames={str(SETUP):0,str(WAKE):60},probe_frame=40,
                geometry='CTF06 floor y=-1.25 and ceiling near y3 leave falling space below staged body origin y1.',
                limitations='Two unseated NPCs under gravity only. No preexisting nonzero velocity, vehicles, props, driven motion, saves or audiovisual-output claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());return path,recipe


def replay():return b'RFI6'+U(48)+bytes(FRAMES*48)


def live_probe(monitor,mapping):
    values={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    values['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return values


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    before,end=result['probe'],result['extra']
    if not 40<=before['frame']<60:raise RuntimeError('Missed frozen-physics window')
    frozen=before['rf_scene_physics_state'];awake=end['rf_scene_physics_state']
    if frozen[:5]!=[2,2,0,0,ACTORS[-1]] or frozen[7] or awake[:5]!=[4,2,2,0,ACTORS[-1]] or awake[7]:
        raise RuntimeError(f'Ordinary linked freeze/wake callbacks failed: {frozen}, {awake}')
    observations=[]
    for i,actor in enumerate(recipe['actors']):
        pre=before['rf_scene_physics_npcs'][i*16:i*16+16]
        post=end['rf_scene_physics_npcs'][i*16:i*16+16]
        if pre[0]!=actor['uid'] or post[:2]!=pre[:2] or pre[1]==0xffffffff or pre[2]&0x18000000!=0x18000000 or pre[3]&0x4000 or post[3]&0x4000:
            raise RuntimeError(f'Actor frozen/visible identity invalid: {pre}, {post}')
        if pre[11]!=1 or post[11] or not 40<=pre[12]<60 or not 110<=post[12]<=FRAMES:
            raise RuntimeError(f'Wrong scripted physics state or stale sample: {pre}, {post}')
        initial=[f(v) for v in pre[4:7]];final=[f(v) for v in post[4:7]]
        if not all(math.isfinite(f(v)) for v in pre[4:11]+post[4:11]+pre[13:]+post[13:]):
            raise RuntimeError('Nonfinite live physical state')
        if max(abs(initial[k]-actor['position'][k]) for k in range(3))>.0001 or any(abs(f(v))>.00001 for v in pre[7:10]+pre[13:16]):
            raise RuntimeError(f'Frozen actor moved or retained linear/angular motion: {pre}')
        if final[1]>=initial[1]-.5 or final[1]<-1.5 or max(abs(final[k]-initial[k]) for k in (0,2))>.2:
            raise RuntimeError(f'Ordinary gravity did not resume at original horizontal pose: {initial}, {final}')
        if pre[10]!=post[10] or f(pre[10])!=actor['details']['authored_health']:
            raise RuntimeError('Physics toggle or short drop changed actor health')
        observations.append(dict(uid=actor['uid'],handle=pre[1],frozen_position=initial,awake_position=final,
                                 frozen_flags=pre[2],final_flags=post[2],health=f(post[10])))
    if frozen[5]!=before['rf_scene_physics_npcs'][17] or awake[5]!=end['rf_scene_physics_npcs'][17]:
        raise RuntimeError('Last callback did not retain second linked NPC identity')
    for sample in (before,end):
        if sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_combat'][0] or any(sample['rf_scene_script_slays']):
            raise RuntimeError('Unexpected combat/death contaminated physics fixture')
    if end['rf_scene_setup_result']!=[2,WAKE,3,0]:raise RuntimeError('Ordinary Invert OFF request failed')
    return dict(result='PASS',actors=observations,freeze_calls=frozen[1],wake_calls=awake[2],
                free_pages=result['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('scripted-physics-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SETUP,WAKE));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'physics')
        result=run_guest(folder,'physics',hdd,FRAMES,360,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=40,allow_guest_error=True)
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
