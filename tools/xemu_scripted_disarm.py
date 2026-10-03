"""Bounded Xbox living Drop_Weapon event, finite-charge drop and idempotence.

Original L1S1 guard8323 and event9494 retain their loadout and14.5s delay in
empty CTF06. A second event copy differs only by UID and executes60frames later.
No death, inventory injection, campaign traversal, images or host input.
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
from xemu_npc_actor_interception import command
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR,DISARM=8323,9494
SETUP,PACIFY,REPEAT=914700,914701,914702
FRAMES=970
SYMBOLS={'rf_scene_scripted_disarm':12,'rf_scene_scripted_disarm_apply':32,
         'rf_scene_scripted_disarm_live':16,'rf_scene_weapon_drops':8,
         'rf_scene_enemy_combat':8,'rf_scene_combat':8,'rf_scene_setup_result':4,
         'rf_scene_script_slays':6,'rf_scene_live_death_audio':4}


def original_records():
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl')
    actor=next(r for r in entity_rows(source) if r['uid']==ACTOR)
    if actor['name']!='env_guard' or b'Riot Stick' not in actor['raw'] or record_details(actor)['seat_host_uid']!=-1:
        raise RuntimeError('Original unseated Riot Stick guard differs')
    meta=inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='L1S1.rfl'))
    sec=next(s for s in meta['sections'] if s['type']=='0x600')
    data=source[sec['offset']+8:sec['offset']+8+sec['size']]
    pe=pefile.PE(str(ROOT/'Installed_Game/RF.exe'));image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    types=[]
    for address_ in range(0x5a1a3c,0x5a1ba4,4):
        at=struct.unpack_from('<I',image,address_-base)[0]-base
        types.append(image[at:image.index(b'\0',at)].decode('cp1252'))
    authored=next(r for r in inspect_events(data,types) if r['uid']==DISARM)
    if authored['type_index']!=81 or authored['links']!=[ACTOR] or authored['delay']!=14.5:
        raise RuntimeError('Original Drop_Weapon event differs')
    return actor,authored,data[authored['offset']:authored['offset']+authored['bytes']]


def prepare_level(folder):
    actor,authored,disarm=original_records()
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('Expected enemy-free CTF06')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    copied=bytearray(actor['raw']);at=actor['transform']
    copied[at:at+48]=F(4,0,-3,1,0,0,0,0,-1,0,1,0)
    if copied[:at]!=actor['raw'][:at] or copied[at+48:]!=actor['raw'][at+48:]:
        raise RuntimeError('Actor modified beyond transform')
    repeat=bytearray(disarm);struct.pack_into('<I',repeat,0,REPEAT)
    events=U(4)+event(SETUP,'Delay','disarm_setup',(PACIFY,DISARM))+\
        command(PACIFY,'Set_AI_Mode','quiet_original_guard',(ACTOR,),0)+disarm+repeat
    replacements={0x30000:U(1)+copied,0x600:events,0x60000:U(0),0x40000:U(0)}
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
    checked,=entity_rows(out)
    if checked['raw']!=bytes(copied):raise RuntimeError('Actor record failed round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,frames=FRAMES,source='levels1.vpp/L1S1.rfl',
                actor_uid=ACTOR,actor_details=record_details(actor),actor_position=[4,0,-3],
                actor_sha256=hashlib.sha256(actor['raw']).hexdigest(),authored_event=authored,
                event_sha256=hashlib.sha256(disarm).hexdigest(),repeat_uid=REPEAT,
                setup_frames={str(SETUP):0,str(REPEAT):60},expected_apply_frames=[870,930],
                staged_fields=['actor transform','ordinary quiet setup','repeat event UID'],
                geometry='CTF06 floor171 at x4,z-3 is y=-1.25; actor settles through ordinary physics, away from neutral player pickup radius.',
                ammo='Original Riot Stick power-cell clip100; dropped quantity must be positive and no greater than actual recorded supply/clip. No ammo injection.',
                limitations='One quiet living guard; no active pre-disarm firing, player collection, held clutter, physical tumbling, OFF action, save or visual/audio claim.')
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
    if not 840<=before['frame']<870 or any(before['rf_scene_scripted_disarm']) or any(before['rf_scene_scripted_disarm_apply']) or before['rf_scene_weapon_drops'][0]:
        raise RuntimeError('Original14.5second disarm delay not retained')
    stats,apply,live=(end[n] for n in ('rf_scene_scripted_disarm','rf_scene_scripted_disarm_apply','rf_scene_scripted_disarm_live'))
    if stats[:5]!=[2,1,1,1,0] or stats[5:8]!=[ACTOR,apply[2],0] or stats[8] or stats[9]!=0xffffffff or not 15480<=stats[11]<=15520:
        raise RuntimeError(f'Repeated ordinary disarm duplicated or rejected work: {stats}')
    if apply[:2]!=[1,ACTOR] or not 14480<=apply[3]<=14520 or apply[4]==0xffffffff or apply[6:8]!=[0xffffffff,0xffffffff] or not apply[8] or apply[9]:
        raise RuntimeError(f'Living disarm identity/ownership failed: {apply}')
    if apply[10:12]!=[1,apply[4]] or not 0<apply[12]<=100 or apply[13]!=apply[14] or f(apply[14])!=recipe['actor_details']['authored_health']:
        raise RuntimeError('Disarm did not preserve health and emit one bounded original weapon charge')
    if apply[15]!=apply[16] or any(apply[i] for i in (18,20,22,23,31)):
        raise RuntimeError('Disarm changed route or retained pending weapon fire')
    if not all(math.isfinite(f(v)) for v in apply[25:31]):raise RuntimeError('Invalid emitted drop/body position')
    drops=end['rf_scene_weapon_drops']
    if drops[0:3]!=[1,0,0] or drops[4]!=1 or drops[7]:
        raise RuntimeError(f'Duplicate/consumed/missing world drop: {drops}')
    if live[:10]!=[1,ACTOR,apply[2],0xffffffff,0xffffffff,0,1,apply[4],apply[12],apply[14]] or live[10]!=apply[16] or any(live[11:]):
        raise RuntimeError(f'Living NPC regained inventory, held model or queued fire: {live}')
    for sample in (before,end):
        if sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_combat'][0] or any(sample['rf_scene_script_slays']) or any(sample['rf_scene_live_death_audio']):
            raise RuntimeError('Unexpected fire, death or runtime error')
    if end['rf_scene_setup_result']!=[2,REPEAT,81,0]:raise RuntimeError('Ordinary repeated event request failed')
    return dict(result='PASS',actor_uid=ACTOR,event_uid=DISARM,repeat_uid=REPEAT,
                applied_ms=apply[3],repeat_ms=stats[11],living_health=f(apply[14]),
                dropped_weapon=apply[11],dropped_charge=apply[12],world_drop_count=drops[0],
                idempotent_repeats=stats[3],held_weapon_absent=True,queued_fire_empty=True,
                free_pages=result['free_pages'],limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('scripted-disarm-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SETUP,REPEAT));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'disarm')
        result=run_guest(folder,'disarm',hdd,FRAMES,720,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=840,allow_guest_error=True)
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
