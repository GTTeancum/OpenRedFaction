"""Bounded Xbox autonomous turret attack against the real player owner.

One authored Stationary Turret in CTF06 faces the unchanged player spawn.
No catatonic/damage fixture, scripted attack, campaign route, host input or
images. Neutral replay allows ordinary acquisition, aim, cadence and hitscan.
Player death is an expected possible result only for this attack harness.
"""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry, U
from xemu_turret_combat import UID, entity_rows, prepare_level, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FRAMES=120
SYMBOLS={'rf_scene_turret_combat':10, 'rf_scene_turret_shots':8, 'rf_scene_turret_owners':8,
         'rf_scene_turret_resources':6, 'rf_scene_turret_draw':4,
         'rf_scene_turret_test':22, 'rf_scene_player_vitals':6,
         'rf_scene_pickup_vitals':4, 'rf_scene_enemy_combat':8}


def prepare_attack(folder):
    path=prepare_level(folder)
    recipe=json.loads((folder/'recipe.json').read_text())
    rows=entity_rows(read_entry(path,'ctf06.rfl'))
    if len(rows)!=1 or rows[0]['uid']!=UID:raise ValueError('Unexpected attack owner')
    transform=struct.unpack_from('<12f',rows[0]['raw'],rows[0]['transform'])
    # Disk transform is position, forward, right, up. The reused fixture
    # already faces +Z from four metres behind the original player spawn.
    to_player=[recipe['spawn'][i]-transform[i] for i in range(3)]
    dot=sum(to_player[i]*transform[3+i] for i in range(3))
    distance=math.sqrt(sum(v*v for v in to_player))
    if not distance or dot/distance<.95:raise ValueError('Turret not facing player spawn')
    recipe.update(staged_fields=['transform'],
        scope='Autonomous authored Vauss acquisition/aim/cadence and real player damage',
        runtime_fixture=False,neutral_frames=FRAMES,forward_cosine=dot/distance,
        expected='Turret acquires player and ordinary shots reduce player health; death permitted')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path


def validate(result):
    extra=result['extra'];combat=extra['rf_scene_turret_combat']
    owners=extra['rf_scene_turret_owners'];draw=extra['rf_scene_turret_draw']
    initial=extra['rf_scene_player_vitals'];final=extra['rf_scene_pickup_vitals']
    enemy=extra['rf_scene_enemy_combat'];shots=extra['rf_scene_turret_shots']
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024:
        raise RuntimeError('Missing complete bounded stock64MiB attack run')
    if any(extra['rf_scene_turret_test']):raise RuntimeError('Catatonic damage fixture unexpectedly active')
    if not combat[0] or not combat[1] or not combat[3] or combat[7]!=UID or combat[8]==0xffffffff or combat[9]:
        raise RuntimeError(f'Autonomous acquisition/fire failed: {combat}')
    if shots[0]!=combat[3] or not shots[1] or shots[2] or shots[3] or shots[6]!=combat[8] or shots[7]:
        raise RuntimeError(f'Turret shots did not damage acquired player: {shots}, {combat}')
    if owners[0]!=1 or owners[2] or owners[3] or owners[7]:
        raise RuntimeError(f'Attacking turret owner damaged or invalid: {owners}')
    health_before=f(initial[0]);health_after=f(final[0])
    if not math.isfinite(health_after) or health_before<=0 or health_after>=health_before:
        raise RuntimeError(f'Real player health did not decrease: {health_before} -> {health_after}')
    if final[2]:raise RuntimeError(f'Fixture player unexpectedly gained health: {final}')
    # CTF06 retains authored pickups: an armor refill does not invalidate
    # actual attributed turret hits or the required net health decrease.
    armor_restored=f(final[3])
    if not math.isfinite(armor_restored) or armor_restored<0:
        raise RuntimeError(f'Invalid armor-pickup telemetry: {final}')
    if enemy[7] or not draw[0] or draw[1] or draw[2] or draw[3]!=UID:
        raise RuntimeError(f'Attack/draw error: {enemy}, {draw}')
    if extra['rf_scene_turret_resources'][0:2]!=[2,1] or result['free_pages']<=0:
        raise RuntimeError('Missing live/replacement resources or free memory')
    return dict(result='PASS',acquisitions=combat[1],shots=combat[3],damaging_hits=shots[1],turns=combat[2],
        cover_holds=combat[4],aim_holds=combat[5],initial_health=health_before,final_health=health_after,
        initial_armor=f(initial[1]),final_armor=f(final[1]),player_death_observed=bool(result['player_life'][2]),
        health_restored=f(final[2]),armor_restored=armor_restored,
        armor_pickup_observed=armor_restored>0,
        free_pages=result['free_pages'],live_submissions=draw[0],
        limitations='One isolated autonomous encounter; no campaign traversal, save/load, screenshots or audible-output inspection.')


def validate_existing(folder):
    """Recheck recorded native attack without altering its original FAIL report."""
    path=folder/'report.json';original_bytes=path.read_bytes();original=json.loads(original_bytes)
    if not original.get('disc_restored'):raise RuntimeError('Disc restoration not confirmed')
    native=json.loads((folder/'fire/result.json').read_text())
    if native!=original.get('native'):raise RuntimeError('Recorded native evidence differs from original report')
    result=validate(native)
    if result['shots']!=2 or result['damaging_hits']!=2:
        raise RuntimeError('Expected the recorded two-shot/two-hit encounter')
    result.update(result='PASS_TURRET_AUTOFIRE',native_rerun=False,
        original_report_result=original['result'],original_report_preserved=True,
        original_report_sha256=hashlib.sha256(original_bytes).hexdigest(),disc_restored=True,
        scope='Recorded autonomous acquisition, aiming, two attributed damaging hits and net player health decrease',
        caveat='Original FAIL retained: the first validator incorrectly forbade an authored CTF06 armor pickup. This evidence includes that armor refill; no health refill occurred, and turret damage still reduced player health.')
    if path.read_bytes()!=original_bytes:raise RuntimeError('Original report changed during validation')
    (folder/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(folder/'validation.json',result['result'],flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Only prepare copied local assets; no build/emulator')
    parser.add_argument('--validate-existing',type=Path,help='Validate recorded attack evidence only; preserve original report')
    args=parser.parse_args()
    if args.validate_existing:
        validate_existing(args.validate_existing);return
    if args.prepare_only:
        print(prepare_attack(args.prepare_only));return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('turret-fire-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_attack(folder/'level')
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
        # Deliberately NO campaign-turret-test.bin: owner must run real AI.
        build(folder,'fire')
        result=run_guest(folder,'fire',hdd,FRAMES,360,snapshot=True,
                         extra_symbols=SYMBOLS,allow_player_dead=True)
        report['native']=result;report.update(validate(result))
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
