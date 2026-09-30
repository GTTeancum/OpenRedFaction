"""Xbox generated head-to-base coupled death through shared firearm hits.

One real authored Auto Turret base creates its child. The opt-in process-local
fixture resolves that generated head and stages handgun-valued collision rays
after frame60; no fake head row, health writes, script Slay, images or host input.
This validates direct-hit/death services, not player aim/input/ammo/cadence.
"""
import datetime
import json
import math
from xemu_auto_turret import BASE, prepare_level, SYMBOLS as AUTO_SYMBOLS, U, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=120
SYMBOLS=dict(AUTO_SYMBOLS,rf_scene_turret_generated_death_probe=10,
    rf_scene_turret_generated_damage_probe=10,rf_scene_turret_generated_test=20,
    rf_scene_setup_result=4,rf_scene_script_slays=6,
    rf_scene_turret_death_effects=8,rf_scene_profile_stage=4,
    rf_scene_turret_retirement=6)


def postdeath_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB head-death run')
    live=result['probe'];final=result['extra'];dead=final['rf_scene_turret_generated_death_probe']
    t=final['rf_scene_turret_generated_test'];request=final['rf_scene_turret_generated_damage_probe']
    if t[:2]!=[BASE,1] or t[2]==t[3] or 0xffffffff in t[2:4] or not t[5] or t[4]<t[5] or t[14] or t[19]!=t[3]:
        raise RuntimeError(f'Generated real-owner firearm contacts failed: {t}')
    if f(t[7])!=60 or f(t[8])!=80 or f(t[9])>0 or f(t[10])>0 or t[11]!=1 or not 60<=t[13]<90 or not t[12]:
        raise RuntimeError(f'Expected live head/base did not enter coupled death: {t}')
    if not 0<=t[16]<=10 or not math.isfinite(f(t[17])) or f(t[17])<=0 or f(t[18])<=0:
        raise RuntimeError('Missing actual handgun damage metadata/accounting')
    if dead[:3]!=[BASE,1,2] or f(dead[3])!=f(t[10]) or f(dead[4])!=f(t[9]) or dead[5:7]!=[1,0xffffffff] or dead[7]!=t[12] or dead[8]!=1 or dead[9]:
        raise RuntimeError(f'Head death did not terminate/detach the real pair: {dead}')
    if request[:2]!=[1,t[2]] or f(request[2])!=1000 or request[3:5]!=[0xffffffff,0xffffffff] or f(request[5])!=80 or f(request[6])!=f(t[10]) or f(request[7])<=0 or request[8:]!=[0,1]:
        raise RuntimeError(f'Expected unique 1000/source-1/kind-1 shared base request missing: {request}')
    if live['rf_scene_profile_stage'][0]<90 or live['rf_scene_turret_generated_test'][11]!=1:
        raise RuntimeError('Missing postdeath observation')
    for extra in (live,final):
        stats=extra['rf_scene_turret_generated_stats'];shots=extra['rf_scene_turret_shots'];combat=extra['rf_scene_turret_combat']
        effects=extra['rf_scene_turret_death_effects'];owners=extra['rf_scene_turret_owners']
        if stats[0]!=1 or not stats[4] or stats[5:]!=[0,1,0]:raise RuntimeError(f'Duplicate/recursive generated death: {stats}')
        if shots[0]!=dead[7] or combat[3]!=dead[7] or shots[7] or combat[9]:raise RuntimeError('Head fired after death or combat failed')
        if effects[0]!=1 or effects[5] or effects[7]:raise RuntimeError(f'Duplicate/error terminal presentation: {effects}')
        if owners[0]!=1 or owners[2]!=t[5] or owners[3]!=1 or owners[7]:raise RuntimeError('Head damage/terminal transitions mismatched')
        if extra['rf_scene_enemy_combat'][2] or extra['rf_scene_enemy_combat'][7]:raise RuntimeError('Base fired handheld or AI failed')
        for name in ('rf_scene_setup_result','rf_scene_script_slays','rf_scene_turret_test','rf_scene_turret_retirement'):
            if any(extra[name]):raise RuntimeError(f'Unexpected script/static fixture/removal path: {name}')
        if extra['rf_scene_turret_generated_death_probe']!=dead or extra['rf_scene_turret_generated_damage_probe']!=request:
            raise RuntimeError('Terminal/request telemetry changed after death')
        if extra['rf_scene_turret_generated_test'][5]!=t[5]:raise RuntimeError('Diagnostic fired again after terminal transition')
    return dict(result='PASS',base_uid=BASE,head_handle=t[3],head_hits=t[5],head_death_frame=t[13],
        base_damage_requests=request[0],base_requested_damage=f(request[2]),base_source=-1,base_kind=-1,
        base_applied_damage=f(request[7]),base_health=f(t[10]),head_health=f(t[9]),
        base_death_dispatches=0,head_death_dispatches=1,death_effect_transitions=1,
        shots_before_death=t[12],shots_after_death=0,free_pages=result['free_pages'],
        limitations='Staged collision rays use real firearm/contact/damage; player aim/input/ammo/cadence, death saves and visual/audio output not checked.')


def main():
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-head-death-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    path=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-auto-head-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(path.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-auto-head-test.bin').write_bytes(U(BASE))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'head-death')
        result=run_guest(folder,'head-death',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
            probe=postdeath_probe,probe_frame=90)
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
