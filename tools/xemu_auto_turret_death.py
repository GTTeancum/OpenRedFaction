"""Xbox generated-head coupled death from normal scripted Slay of its base.

The existing Auto Turret fixture creates its child naturally. A synthetic
Slay_Object event targets authored base1994 at frame60; no head record or
direct damage injection. Runs120 neutral frames, no images or host input.
"""
import argparse
import datetime
import io
import json
import math
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

START,SLAY=911940,911941
FRAMES,DEATH_FRAME=120,60
SYMBOLS=dict(AUTO_SYMBOLS,rf_scene_turret_generated_death_probe=10,
             rf_scene_setup_result=4,rf_scene_script_slays=6,
             rf_scene_turret_death_effects=8,rf_scene_profile_stage=4)


def prepare_level(folder):
    path=prepare_base(folder);original=read_entry(path,'ctf06.rfl')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    payload=U(2)+event(START,'Delay','auto_death_start')+event(SLAY,'Slay_Object','auto_base_slay',(BASE,))
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
        raise RuntimeError('Death fixture altered authored base/head population')
    archive(path,[('ctf06.rfl',out)])
    recipe=json.loads((folder/'recipe.json').read_text())
    recipe.update(scope=__doc__,staged_events=[dict(uid=START,type='Delay',links=[],frame=0),
        dict(uid=SLAY,type='Slay_Object',links=[BASE],frame=DEATH_FRAME)],neutral_frames=FRAMES)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');return path


def postdeath_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB death run')
    live=result['probe'];final=result['extra'];dead=final['rf_scene_turret_generated_death_probe']
    if live['rf_scene_profile_stage'][0]<DEATH_FRAME+10:raise RuntimeError('Missing postdeath observation')
    if dead[:3]!=[BASE,1,1] or not math.isfinite(f(dead[3])) or f(dead[3])>0 or dead[4]!=0 or dead[5:7]!=[1,0xffffffff] or dead[9]:
        raise RuntimeError(f'Base death did not terminate/detach generated head: {dead}')
    if not dead[7] or dead[8]!=1:raise RuntimeError('Missing prior ordinary head fire or unique terminal effect transition')
    for extra in (live,final):
        stats=extra['rf_scene_turret_generated_stats'];shots=extra['rf_scene_turret_shots'];combat=extra['rf_scene_turret_combat']
        slay=extra['rf_scene_script_slays'];effects=extra['rf_scene_turret_death_effects']
        if stats[0]!=1 or not stats[4] or stats[5:]!=[1,0,0]:raise RuntimeError(f'Duplicate/missing generated death dispatch: {stats}')
        if extra['rf_scene_setup_result']!=[2,SLAY,1,0] or slay[:3]!=[1,1,BASE] or f(slay[3])>0 or slay[5]:
            raise RuntimeError(f'Scripted base Slay failed: {slay}')
        if shots[0]!=dead[7] or combat[3]!=dead[7] or shots[7] or combat[9]:
            raise RuntimeError('Generated head continued firing after base death or combat failed')
        if effects[0]!=dead[8] or effects[5] or effects[7]:raise RuntimeError(f'Duplicate/error terminal presentation: {effects}')
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7] or any(extra['rf_scene_turret_test']):
            raise RuntimeError('Base handheld shot/error or synthetic damage fixture detected')
        if extra['rf_scene_turret_generated_death_probe']!=dead:raise RuntimeError('Terminal probe changed after death')
    if final['rf_scene_turret_owners'][3]!=1 or final['rf_scene_turret_owners'][7]:raise RuntimeError('Head terminal owner transition missing/duplicated')
    return dict(result='PASS',base_uid=BASE,death_frame=DEATH_FRAME,base_death_dispatches=1,
        head_death_dispatches=0,base_health=f(dead[3]),head_health=0,head_dead=True,
        shots_before_death=dead[7],shots_after_death=0,head_death_effect_transitions=dead[8],
        free_pages=result['free_pages'],
        limitations='Base-to-head lethal coupling only; head-to-base damage, save/load, pure removal and visual/audio output not checked.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare copied assets/events only; no build or emulator')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-death-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    path=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(path.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(START,SLAY))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'death')
        result=run_guest(folder,'death',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
                         allow_player_dead=True,probe=postdeath_probe,probe_frame=75)
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
