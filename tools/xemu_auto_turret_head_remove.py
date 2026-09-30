"""Xbox head-only Remove_Object through a real registered runtime event.

The authored Auto Turret base creates its actual head. At frame60 an opt-in
diagnostic resolves that generated key into the type2 event link dispatcher.
No fake head RFL row, health writes, base removal, images or host input.
"""
import datetime
import json
from xemu_auto_turret import BASE, prepare_level, SYMBOLS as AUTO_SYMBOLS, U, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=120
SYMBOLS=dict(AUTO_SYMBOLS,rf_scene_turret_head_remove_test=24,
    rf_scene_turret_generated_death_probe=10,rf_scene_turret_generated_damage_probe=10,
    rf_scene_turret_retirement=6,rf_scene_turret_retirement_probe=8,rf_scene_actor_retirement=4,
    rf_scene_setup_result=4,rf_scene_script_slays=6,rf_scene_turret_death_effects=8,
    rf_scene_profile_stage=4)


def postremove_probe(monitor,mapping):
    return {name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB head removal run')
    live=result['probe'];final=result['extra'];t=final['rf_scene_turret_head_remove_test']
    if live['rf_scene_profile_stage'][0]<75:raise RuntimeError('Missing post-removal observation')
    for x in (live,final):
        p=x['rf_scene_turret_head_remove_test'];stats=x['rf_scene_turret_generated_stats'];r=x['rf_scene_turret_retirement_probe']
        if p[:2]!=[BASE,1] or p[2]==p[3] or 0xffffffff in p[2:4] or p[4:6]!=[1,1]:
            raise RuntimeError(f'Head-only event did not retain distinct live registrations: {p}')
        if f(p[6])!=80 or f(p[7])!=60 or p[8]&(2|0x4000) or p[9]&(2|0x4000)!=(2|0x4000) or p[10:14]!=[0xffffffff,0,2,0]:
            raise RuntimeError(f'Removal damaged/retired the base or failed to hide/detach its head: {p}')
        if p[14:18]!=[1,0,0,0] or p[20:]!=[1,0,1,0xffffffff] or not p[19]:
            raise RuntimeError(f'Actual type2 event dispatch/control shutdown failed: {p}')
        if x['rf_scene_turret_retirement']!=[0,1,0,BASE,2,0] or x['rf_scene_actor_retirement'][1] or x['rf_scene_actor_retirement'][3]:
            raise RuntimeError('Head-only retirement removed the base or repeated/failed')
        if r[:3]!=[BASE,1,1] or r[3]!=p[9] or r[4]!=0xffffffff or f(r[5])!=60 or r[6] or r[7]!=p[19]:
            raise RuntimeError('Retirement owner snapshot differs from the live pair')
        if stats[0]!=1 or not stats[4] or any(stats[5:]):raise RuntimeError('Generated death/error occurred')
        if x['rf_scene_turret_shots'][0]!=p[19] or x['rf_scene_turret_combat'][3]!=p[19] or x['rf_scene_turret_shots'][7] or x['rf_scene_turret_combat'][9]:
            raise RuntimeError('Removed head continued firing or combat failed')
        for name in ('rf_scene_turret_generated_death_probe','rf_scene_turret_generated_damage_probe','rf_scene_turret_death_effects','rf_scene_setup_result','rf_scene_script_slays','rf_scene_turret_test'):
            if any(x[name]):raise RuntimeError(f'Unexpected death/damage/script/static fixture: {name}')
        owners=x['rf_scene_turret_owners']
        if owners[0]!=1 or owners[2] or owners[3] or owners[7] or x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7]:
            raise RuntimeError('Unexpected damage/death/handheld shot or owner failure')
    if final['rf_scene_turret_draw']!=live['rf_scene_turret_draw'] or not final['rf_scene_turret_draw'][0] or any(final['rf_scene_turret_draw'][1:3]):
        raise RuntimeError('Removed head kept drawing or selected a death model')
    return dict(result='PASS',base_uid=BASE,base_registered=True,base_health=f(t[6]),base_retired=False,
        head_registered_tombstone=True,head_health=f(t[7]),head_dead=False,head_hidden=True,
        runtime_remove_events=1,temporary_event_removed=True,shots_before_removal=t[19],shots_after_removal=0,
        death_dispatches=0,death_effects=0,free_pages=result['free_pages'],
        limitations='One head-only runtime-linked Remove_Object; persistence and visual/audio output not checked.')


def main():
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-head-remove-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    path=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-auto-head-remove.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(path.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-auto-head-remove.bin').write_bytes(U(BASE))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'head-remove')
        result=run_guest(folder,'head-remove',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS,
            probe=postremove_probe,probe_frame=75)
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
