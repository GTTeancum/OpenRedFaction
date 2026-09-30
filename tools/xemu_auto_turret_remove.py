"""Xbox scripted removal of an authored Auto Turret base and its generated head.

Normal activation/fire precedes Remove_Object at frame60. The base unregisters;
its head remains an inert hidden tombstone without lethal damage/death effects.
No fake head RFL record, images, host input, campaign route or direct damage.
"""
import argparse
import datetime
import io
import json
from pathlib import Path
import struct
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from build_fragment_platform_fixture import read_entry, U
from inspect_levels import inspect
from xemu_auto_turret import BASE, prepare_level as prepare_base, SYMBOLS as AUTO_SYMBOLS, f
from xemu_turret_combat import entity_rows
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

START,REMOVE=911950,911951
FRAMES,REMOVE_FRAME=120,60
SYMBOLS=dict(AUTO_SYMBOLS,rf_scene_turret_generated_death_probe=10,
             rf_scene_turret_retirement=6,rf_scene_actor_retirement=4,rf_scene_turret_retirement_probe=8,
             rf_scene_setup_result=4,rf_scene_script_slays=6,
             rf_scene_turret_death_effects=8,rf_scene_profile_stage=4)


def prepare_level(folder):
    path=prepare_base(folder);original=read_entry(path,'ctf06.rfl')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    payload=U(2)+event(START,'Delay','auto_remove_start')+event(REMOVE,'Remove_Object','auto_base_remove',(BASE,))
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    present={int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0 and 0x600 not in present:
            offsets[0x600]=len(out);out+=U(0x600,len(payload))+payload;added=1
        data=payload if kind==0x600 else original[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind]=len(out);out+=U(kind,len(data))+data
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    if [(r['uid'],r['name']) for r in entity_rows(out)]!=[(BASE,'Auto Turret')]:
        raise RuntimeError('Removal fixture altered authored base/head population')
    archive(path,[('ctf06.rfl',out)])
    recipe=json.loads((folder/'recipe.json').read_text())
    recipe.update(scope=__doc__,staged_events=[dict(uid=START,type='Delay',links=[],frame=0),
        dict(uid=REMOVE,type='Remove_Object',links=[BASE],frame=REMOVE_FRAME)],neutral_frames=FRAMES)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');return path


def postremove_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB removal run')
    live=result['probe'];final=result['extra'];removed=final['rf_scene_turret_retirement_probe']
    if live['rf_scene_profile_stage'][0]<REMOVE_FRAME+10:raise RuntimeError('Missing post-removal observation')
    # Retirement publishes before the caller unregisters the base. The later
    # actor_retirement count below advances only after unregister/body close.
    if removed[:3]!=[BASE,1,1] or removed[3]&(2|0x4000)!=(2|0x4000) or removed[4]!=0xffffffff or f(removed[5])!=60 or removed[6] or not removed[7]:
        raise RuntimeError(f'Removal did not hide/detach the living generated head: {removed}')
    for extra in (live,final):
        retirement=extra['rf_scene_turret_retirement'];actors=extra['rf_scene_actor_retirement']
        stats=extra['rf_scene_turret_generated_stats'];shots=extra['rf_scene_turret_shots'];combat=extra['rf_scene_turret_combat']
        if retirement!=[1,0,0,BASE,1,0] or actors[1]!=1 or actors[3]:
            raise RuntimeError(f'Unique authored base removal failed: {retirement}, {actors}')
        if stats[0]!=1 or not stats[4] or any(stats[5:]):raise RuntimeError(f'Removal invoked coupled death or failed: {stats}')
        if extra['rf_scene_setup_result']!=[2,REMOVE,2,0]:raise RuntimeError('Remove_Object event did not dispatch normally')
        if any(extra['rf_scene_script_slays']) or any(extra['rf_scene_turret_generated_death_probe']) or any(extra['rf_scene_turret_death_effects']):
            raise RuntimeError('Removal manufactured lethal damage/death presentation')
        if shots[0]!=removed[7] or combat[3]!=removed[7] or shots[7] or combat[9]:
            raise RuntimeError('Retired generated head continued firing or combat failed')
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7] or any(extra['rf_scene_turret_test']):
            raise RuntimeError('Base handheld shot/error or synthetic damage fixture detected')
        if extra['rf_scene_turret_retirement_probe']!=removed:raise RuntimeError('Retirement probe changed after removal')
        owners=extra['rf_scene_turret_owners']
        if owners[0]!=1 or owners[2] or owners[3] or owners[7]:raise RuntimeError('Head received damage or duplicate terminal processing')
    if final['rf_scene_turret_draw']!=live['rf_scene_turret_draw'] or not final['rf_scene_turret_draw'][0] or any(final['rf_scene_turret_draw'][1:3]):
        raise RuntimeError('Head continued drawing after retirement or selected a death model')
    return dict(result='PASS',base_uid=BASE,removal_frame=REMOVE_FRAME,base_unregistered=True,
        head_registered_tombstone=True,head_hidden=True,head_detached=True,head_health=f(removed[5]),
        head_dead=False,shots_before_removal=removed[7],shots_after_removal=0,
        death_dispatches=0,death_effects=0,free_pages=result['free_pages'],
        limitations='Authored base Remove_Object and inert child only; head-only removal, persistence and visual/audio output not checked.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare copied assets/events only; no build or emulator')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-remove-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    path=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(path.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(START,REMOVE))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'remove')
        result=run_guest(folder,'remove',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
                         allow_player_dead=True,probe=postremove_probe,probe_frame=75)
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
