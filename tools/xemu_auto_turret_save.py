"""Bounded Xbox generated-head ordinary save and fresh-load continuation.

One authored Auto Turret base1994 in the existing isolated CTF06 fixture.
Sixty neutral source frames and twelve load frames; no host input, images,
campaign route, forced readiness, damage injection or player-death allowance.
Parent owns serial builds/emulator execution. Coupled-dead saves are deferred.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_auto_turret import BASE, prepare_level, SYMBOLS as AUTO_SYMBOLS, U, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES, LOAD_FRAMES = 60, 12
SYMBOLS = dict(AUTO_SYMBOLS, rf_scene_turret_generated_restore_probe=32,
               rf_scene_turret_death_effects=8, rf_scene_world_load_reject=3,
               rf_scene_npc_checkpoint_reject_state=6)


def saved_row(payload):
    if len(payload)<320 or payload[:4]!=b'RFWC':
        raise RuntimeError('Missing ordinary world checkpoint')
    kind, offset, size=struct.unpack_from('<III',payload,128+10*12)
    if kind!=11 or offset<320 or offset+size>len(payload):
        raise RuntimeError('Invalid vehicle component')
    block=payload[offset:offset+size]
    if len(block)!=232 or block[:4]!=b'RFTU' or struct.unpack_from('<III',block,4)!=(2,0,1):
        raise RuntimeError('Expected RFTU2 with one generated row and empty inner vehicle payload')
    row=block[16:];base,role=struct.unpack_from('<II',row)
    v=row[8:192];uid,cls=struct.unpack_from('<II',v)
    health,armor=struct.unpack_from('<ff',v,8)
    dead,flags,flags810=struct.unpack_from('<III',v,16)
    action=struct.unpack_from('<i',v,36)[0]
    target,remaining,burst=struct.unpack_from('<III',v,40)
    combat_action=struct.unpack_from('<i',v,60)[0]
    base_health,base_flags,base_death,dispatch,target_role,head_flags=struct.unpack_from('<f5I',row,192)
    basis=list(struct.unpack_from('<9I',v,76));angles=list(struct.unpack_from('<3I',v,112))
    position=list(struct.unpack_from('<3I',v,64))
    if (base,role,uid,dead,dispatch,target_role,target)!=(BASE,1,0xffffffff,0,0,0,0):
        raise RuntimeError('Saved generated identity/live state/player target differs from fixture')
    if base_health!=80 or not base_flags&1 or base_death&1 or not head_flags&0x100000:
        raise RuntimeError('Saved base readiness/vitals or generated-head flags invalid')
    if not math.isfinite(health) or health<=0 or not math.isfinite(armor) or armor<0 or flags810&1 or not flags&0x100:
        raise RuntimeError('Saved generated head vitals/independent basis flag invalid')
    if remaining>36000 or not all(math.isfinite(f(w)) for w in basis+angles+position):
        raise RuntimeError('Invalid saved pose or remaining cadence')
    if not any(abs(f(w))>1e-5 for w in angles):raise RuntimeError('Source head never aimed')
    return dict(base_uid=base,role=role,head_uid=uid,class_index=cls,health=health,armor=armor,
        dead=dead,action=action,target_role=target_role,target_uid=target,remaining=remaining,
        burst=burst,combat_action=combat_action,base_health=base_health,base_flags=base_flags,
        death_dispatched=dispatch,basis_words=basis,angle_words=angles,position_words=position)


def check_live(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    if result['player_life'][2]:raise RuntimeError('Player died during bounded save continuation')
    x=result['extra'];p=x['rf_scene_turret_generated_probe'];stats=x['rf_scene_turret_generated_stats']
    if stats[0]!=1 or stats[1]<2 or any(stats[5:]) or p[:2]!=[BASE,1] or p[30:]!=[1,0]:
        raise RuntimeError(f'Generated owner missing, duplicated or dead: {stats}, {p}')
    if p[2]==p[3] or 0xffffffff in p[2:4] or p[4:6]!=[0xffffffff,0xffffffff] or p[6]!=p[2]:
        raise RuntimeError(f'Generated head lost actual parent link: {p}')
    if f(p[7])!=80 or not math.isfinite(f(p[8])) or f(p[8])<=0 or not p[9]&1 or not p[10]&0x100:
        raise RuntimeError('Generated base/head health, readiness or independent basis invalid')
    head=[f(v) for v in p[12:15]];tag=[f(v) for v in p[15:18]]
    if not all(math.isfinite(v) for v in head+tag) or max(abs(a-b) for a,b in zip(head,tag))>1e-5:
        raise RuntimeError('Generated head does not follow the evaluated base interface')
    if not all(math.isfinite(f(w)) for w in p[18:30]):raise RuntimeError('Nonfinite generated/base pose')
    if x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7] or any(x['rf_scene_turret_test']):
        raise RuntimeError('Base handheld fire or synthetic contact fixture ran')
    if x['rf_scene_turret_owners'][0]!=1 or x['rf_scene_turret_owners'][3] or x['rf_scene_turret_owners'][7]:
        raise RuntimeError('Owner count/death/error mismatch')
    if any(x['rf_scene_turret_death_effects']) or x['rf_scene_turret_combat'][9] or x['rf_scene_turret_shots'][7]:
        raise RuntimeError('Duplicate death effect or combat failure')
    if not x['rf_scene_turret_draw'][0] or x['rf_scene_turret_draw'][1] or x['rf_scene_turret_draw'][2]:
        raise RuntimeError('Live generated model did not submit cleanly')
    return p


def validate_source(saved,payload):
    p=check_live(saved,SAVE_FRAMES);row=saved_row(payload);x=saved['extra'];state=saved['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary source save failed: {state}')
    if any(x['rf_scene_turret_generated_restore_probe']):raise RuntimeError('Source unexpectedly restored a generated checkpoint')
    # Publication telemetry samples before this frame's aim update; the wire
    # row and immutable restore probe carry the exact post-combat basis.
    if row['health']!=f(p[8]) or row['base_health']!=f(p[7]):
        raise RuntimeError('Serialized generated health differs from the source owner')
    if not x['rf_scene_turret_combat'][1] or not x['rf_scene_turret_combat'][2]:
        raise RuntimeError('Source did not acquire and independently aim')
    return row


def validate(saved,loaded,payload):
    row=validate_source(saved,payload);p=check_live(loaded,LOAD_FRAMES);x=loaded['extra'];state=loaded['checkpoint_state']
    r=x['rf_scene_turret_generated_restore_probe'];due=row['remaining'] if row['remaining'] else 0
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary generated restore failed: {state}, {x["rf_scene_world_load_reject"]}')
    if r[:3]!=[1,BASE,1] or r[3]!=p[3] or r[4]!=p[2] or r[5] or f(r[6])!=row['health'] or f(r[7])!=row['armor']:
        raise RuntimeError(f'Generated restore count/identity/vitals differ: {r}, {row}')
    if r[8:10]!=[row['target_role'],row['target_uid']] or r[10] in (0,0xffffffff) or r[11:16]!=[row['remaining'],due,0,row['burst'],row['combat_action']&0xffffffff]:
        raise RuntimeError(f'Generated target/cadence did not rebase exactly: {r}, {row}')
    if r[16:25]!=row['basis_words'] or r[25:28]!=row['angle_words'] or r[28:31]!=row['position_words'] or r[31]:
        raise RuntimeError('Generated aim/position/death dispatch did not restore exactly')
    combat=x['rf_scene_turret_combat'];shots=x['rf_scene_turret_shots']
    if not combat[0] or combat[1] or combat[8]!=r[10] or shots[0]!=combat[3]:
        raise RuntimeError('Loaded target was lost/reacquired or combat continuation failed')
    if row['remaining']<LOAD_FRAMES and not shots[0]:
        raise RuntimeError('Restored due cadence did not produce a shot during continuation')
    if shots[0] and (not shots[1] or shots[6]!=r[10]):raise RuntimeError('Continued shots did not damage the restored player target')
    return dict(result='PASS',saved_generated=row,restored_probe=r,
        source_shots=saved['extra']['rf_scene_turret_shots'][0],loaded_shots=shots[0],
        loaded_damaging_hits=shots[1],created_heads=1,duplicate_death_effects=0,
        player_health_at_save=f(saved['extra']['rf_scene_pickup_vitals'][0]),
        player_health_after_load=f(x['rf_scene_pickup_vitals'][0]),
        free_pages=min(saved['free_pages'],loaded['free_pages']),
        limitations='One live generated owner save/fresh-load; coupled-dead saves deferred because this fixture has no clean damage injection. No images, audio or campaign claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing',type=Path,help='Validate saved evidence only; no build/emulator')
    args=parser.parse_args()
    if args.validate_existing:
        folder=args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):raise RuntimeError('Disc restoration not confirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()),
            json.loads((folder/'load/result.json').read_text()),(folder/'save/xbox-world.rfwc').read_bytes()),indent=2));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',scope=__doc__,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS)
        report['phases']['save']=saved;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_generated']=validate_source(saved,payload)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(LOAD_FRAMES*48))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,snapshot=True,extra_symbols=SYMBOLS)
        report['phases']['load']=loaded;report.update(validate(saved,loaded,payload))
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
