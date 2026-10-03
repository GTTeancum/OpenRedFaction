"""Bounded Xbox NPC shot interception without changing the Attack target.

Three complete L3S2 guard941 records in empty CTF06, UID/transform edits only.
Ordinary events neutralize and pacify the blocker/target, Attack the target at90,
then Remove_Object the intervening blocker at120. Real pistol hits must change
victim while retaining source/intended UID. No forced damage, inventory, host
input, images, campaign traversal or original-game runtime.
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
from xemu_turret_combat import entity_rows
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SHOOTER,BLOCKER,TARGET=914400,914401,914402
ROOT_EVENT,NEUTRAL,PACIFY,ATTACK,REMOVE=914410,914411,914412,914413,914414
REMOVE_FRAME,FRAMES=120,180
SYMBOLS={'rf_scene_enemy_actor_hits':113,'rf_scene_enemy_combat':8,
         'rf_scene_script_attack':12,'rf_scene_setup_result':4,
         'rf_scene_combat':8,'rf_scene_script_slays':6,
         'rf_scene_actor_retirement':4}


def command(uid,kind,name,links,word=0,delay=0):
    value=bytearray(event(uid,kind,name,links))
    at=4+2+len(kind)+12+2+len(name)+1
    struct.pack_into('<f',value,at,delay)
    struct.pack_into('<I',value,at+4+2,word)
    return value


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L3S2.rfl')
    guard=next(r for r in entity_rows(source) if r['uid']==941)
    details=record_details(guard)
    if guard['name']!='guard1' or details['seat_host_uid']!=-1 or b'12mm handgun' not in guard['raw']:
        raise RuntimeError('Expected ordinary unseated pistol guard941')
    raw=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(raw):raise RuntimeError('Expected empty actor testbed')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',raw,start['offset']+8)
    copies=[];positions=[]
    # The shooter faces+x. The player remains outside its120degree sight cone,
    # as in the established opposed fixture. Same-height blocker lies on the
    # real eye-to-target ray; runtime collision still determines actual hits.
    for i,uid in enumerate((SHOOTER,BLOCKER,TARGET)):
        position=(spawn[0]+(i-1)*1.5,spawn[1],spawn[2]-3.5)
        value=bytearray(guard['raw']);struct.pack_into('<I',value,0,uid)
        at=guard['transform'];value[at:at+48]=F(*position,1,0,0,0,0,-1,0,1,0)
        copies.append(value);positions.append(position)
    events=U(5)+event(ROOT_EVENT,'Delay','interception_setup',(NEUTRAL,PACIFY,ATTACK))+\
        command(NEUTRAL,'Set_Friendliness','neutral_victims',(BLOCKER,TARGET),1)+\
        command(PACIFY,'Set_AI_Mode','pacify_victims',(BLOCKER,TARGET),0)+\
        command(ATTACK,'Attack','retained_target',(TARGET,),SHOOTER,1.5)+\
        command(REMOVE,'Remove_Object','clear_blocker',(BLOCKER,),delay=1.0)
    replacements={0x30000:U(3)+b''.join(copies),0x600:events,0x60000:U(0),0x40000:U(0)}
    present={int(s['type'],16) for s in meta['sections']}
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={};added=0
    for section in meta['sections']:
        k=int(section['type'],16)
        if k==0:
            for missing in (0x30000,0x600):
                if missing not in present:
                    value=replacements[missing];offsets[missing]=len(out);out+=U(missing,len(value))+value;added+=1
        value=replacements.get(k,raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[k]=len(out);out+=U(k,len(value))+value
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=[bytes(v) for v in copies]:
        raise RuntimeError('Staged authored entities failed round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp'
    archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,source_uid=941,source_details=details,
                source_sha256=hashlib.sha256(guard['raw']).hexdigest(),
                actors=[SHOOTER,BLOCKER,TARGET],positions=positions,changed_fields=['UID','transform'],
                events=dict(setup0=ROOT_EVENT,attack90=ATTACK,remove120=REMOVE),
                assumptions='First shot must intercept a still-living blocker; no geometrical/admission bypass.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path,recipe


def real(word):return struct.unpack('<f',struct.pack('<I',word))[0]


def validate(run,recipe):
    if run['guest_phase']!=5 or run['frames']!=FRAMES or run['memory_bytes']!=64*1024*1024 or run['free_pages']<=0 or run['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB interception run')
    x=run['extra'];log=x['rf_scene_enemy_actor_hits'];count=log[0]
    if not 2<=count<=16:raise RuntimeError(f'Insufficient hits or retained log overflow: {count}')
    rows=[log[1+i*7:8+i*7] for i in range(count)]
    before=[r for r in rows if r[3]<REMOVE_FRAME];after=[r for r in rows if r[3]>=REMOVE_FRAME]
    if not before or not after:raise RuntimeError(f'Missing pre/post-removal real hits: {rows}')
    initial=recipe['source_details']['authored_health']
    for row in rows:
        if row[:2]!=[SHOOTER,TARGET] or not 90<=row[3]<FRAMES:
            raise RuntimeError(f'Source/intended target changed or an opposing NPC fired: {row}')
        if not all(math.isfinite(real(v)) for v in row[4:]) or real(row[4])<=0:
            raise RuntimeError(f'Hit did not apply finite positive damage: {row}')
    if any(r[2]!=BLOCKER or real(r[6])!=initial or real(r[5])<=0 for r in before):
        raise RuntimeError(f'Intervening living actor failed to shield intended target: {before}')
    if any(r[2]!=TARGET for r in after) or not any(real(r[6])<initial and real(r[5])==real(r[6]) for r in after):
        raise RuntimeError(f'Unchanged intended target did not take damage after ordinary removal: {after}')
    attack=x['rf_scene_script_attack'];combat=x['rf_scene_enemy_combat']
    if attack[:3]!=[1,ATTACK,SHOOTER] or attack[5]<count or combat[7] or combat[2]<count:
        raise RuntimeError('Ordinary Attack/shot path failed')
    if x['rf_scene_setup_result']!=[2,REMOVE,2,0] or x['rf_scene_actor_retirement'][3]:
        raise RuntimeError('Ordinary Remove_Object setup failed')
    if any(x['rf_scene_script_slays']) or x['rf_scene_combat'][0] or real(combat[5])!=100:
        raise RuntimeError('Forced death/player shots/player damage contaminated fixture')
    return dict(result='PASS',hits=rows,source_uid=SHOOTER,intended_uid=TARGET,
                first_actual_uid=BLOCKER,later_actual_uid=TARGET,
                limitations='One ordinary pistol actor interception. Shield interception, pellets, visuals and broad tactics are not verified.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-actor-interception-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(ROOT_EVENT,REMOVE))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'interception');report['native']=run_guest(folder,'interception',hdd,FRAMES,360,snapshot=True,
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
