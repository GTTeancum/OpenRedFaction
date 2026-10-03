"""One ordinary Xbox save/fresh-load of autonomous opposed NPC targets.

Delays the existing Set_Friendliness by1s, saves80frames after the two actors
acquire each other but before their first shots, then resumes120frames without
setup replay. RFNC12 reactive targetUIDs, deadlines, RNG and loaded ammunition
must restore exactly before simulation. Real continued combat must end in one
death and target release. No images, host input, forced damage or campaign path.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct

from xemu_npc_opposed import ACTORS,FRIENDLY,SYMBOLS as BASE_SYMBOLS,prepare_level,real
from xemu_npc_jeep_detached_save import component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES,LOAD_FRAMES,PROBE_FRAME=80,120,90
SYMBOLS=dict(BASE_SYMBOLS,rf_scene_enemy_reactive_restore=21,
             rf_scene_npc_checkpoint_reject_state=6,rf_scene_world_load_reject=3,
             rf_scene_actor_landing=8,rf_scene_profile_stage=4)


def saved_rows(payload):
    blob=component(payload,2)
    if len(blob)<64 or blob[:4]!=b'RFNC' or struct.unpack_from('<I',blob,4)[0]!=12:
        raise RuntimeError('Expected RFNC12 reactive combat checkpoint')
    count,=struct.unpack_from('<I',blob,16)
    if count!=2:raise RuntimeError('Expected exactly two saved opposed actors')
    rows=[];at=64
    for i in range(count):
        if at+600>len(blob):raise RuntimeError('Truncated RFNC row')
        p=blob[at:];uid,cls,retired,flags,affiliation=struct.unpack_from('<5I',p)
        health,armor=struct.unpack_from('<2I',p,20)
        weapon,=struct.unpack_from('<i',p,44)
        animation,=struct.unpack_from('<I',p,540)
        move,combat=struct.unpack_from('<2I',p,564)
        shots,=struct.unpack_from('<I',p,588)
        span=600+move+combat+shots*24+animation
        if move not in (0,168) or combat!=40 or shots or at+span>len(blob):
            raise RuntimeError('Unexpected movement/combat/queued-shot row shape')
        c=list(struct.unpack_from('<10I',p,600+move))
        owned=list(p[76:140]);reserve=list(struct.unpack_from('<32i',p,140))
        loaded=list(struct.unpack_from('<64i',p,268))
        if uid!=ACTORS[i] or retired or not 0<=weapon<64 or not owned[weapon] or loaded[weapon]<=0:
            raise RuntimeError('Saved actor identity/weapon ownership changed')
        if any(x<0 for x in reserve+loaded) or c[0]!=4 or c[1]!=ACTORS[1-i] or not 0<c[3]<30 or c[4] or c[2]:
            raise RuntimeError(f'Expected two pending initial reactive attacks: {uid}, {c}')
        rows.append(dict(uid=uid,target_uid=c[1],affiliation=affiliation,
            health_bits=health,armor_bits=armor,weapon=weapon,loaded=loaded[weapon],
            combat=c,inventory_sha256=hashlib.sha256(p[76:524]).hexdigest(),
            owned=owned,reserve=reserve,loaded_inventory=loaded))
        at+=span
    if at!=len(blob) or [r['affiliation'] for r in rows]!=[0,2] or rows[0]['combat'][9]!=rows[1]['combat'][9]:
        raise RuntimeError('Saved affiliation/shared RNG or row boundary mismatch')
    return rows


def probe(monitor,mapping):
    return {n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}


def check_run(run,frames):
    if run['guest_phase']!=5 or run['frames']!=frames or run['memory_bytes']!=64*1024*1024 or run['free_pages']<=0 or run['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB opposed save run')
    x=run['extra']
    if x['rf_scene_enemy_combat'][7] or x['rf_scene_enemy_opposed'][11] or x['rf_scene_enemy_awareness'][7]:
        raise RuntimeError('NPC combat/awareness runtime error')
    if any(x['rf_scene_script_attack']) or any(x['rf_scene_script_slays']) or x['rf_scene_combat'][0]:
        raise RuntimeError('Scripted Attack/Slay or player fire contaminated continuation')


def validate_source(run,payload,details):
    check_run(run,SAVE_FRAMES);x=run['extra'];state=run['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary source save failed: {state}')
    if x['rf_scene_actor_landing'][1]!=1 or x['rf_scene_setup_result']!=[1,FRIENDLY,30,0]:
        raise RuntimeError('Delayed friendliness/grounded source setup did not complete')
    if x['rf_scene_enemy_combat'][2:4]!=[0,0] or any(x['rf_scene_enemy_reactive_restore']):
        raise RuntimeError('Source fired early or was already restored')
    a=x['rf_scene_enemy_opposed']
    if a[2]!=2 or a[12:14]!=[1,1] or x['rf_scene_enemy_awareness'][1]:
        raise RuntimeError(f'Expected two genuine opposed acquisitions: {a}')
    rows=saved_rows(payload)
    for row in rows:
        if real(row['health_bits'])!=details['authored_health'] or real(row['armor_bits'])!=details['authored_armor']:
            raise RuntimeError('Saved actors were already damaged before the intended pause')
    return rows


def validate(source,loaded,payload,details):
    rows=validate_source(source,payload,details);check_run(loaded,LOAD_FRAMES)
    x=loaded['extra'];state=loaded['checkpoint_state'];restored=x['rf_scene_enemy_reactive_restore']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Fresh ordinary load failed: {state}')
    if restored[0]!=2:raise RuntimeError(f'Expected exactly two reactive restores: {restored}')
    for i,row in enumerate(rows):
        c=row['combat'];actual=restored[1+i*10:11+i*10]
        expected=[row['uid'],row['target_uid'],2,c[3],c[4],c[2],c[9],row['health_bits'],row['armor_bits'],row['loaded']]
        if actual!=expected:raise RuntimeError(f'Restore changed target/mode/deadline/RNG/vitals/ammo: {actual} != {expected}')
    if any(x['rf_scene_setup_result']) or x['rf_scene_enemy_opposed'][2] or any(x['rf_scene_enemy_opposed'][12:14]) or x['rf_scene_enemy_awareness'][1]:
        raise RuntimeError('Fresh load replayed setup or reacquired instead of restoring targets')
    pair=x['rf_scene_npc_opposed_probe'];live=[pair[:12],pair[12:]]
    if {r[0] for r in live}!=set(ACTORS) or live[0][1]==live[1][1]:
        raise RuntimeError('Missing distinct restored actor identities')
    for row,other in ((live[0],live[1]),(live[1],live[0])):
        if real(row[4])+real(row[5])>=details['authored_health']+details['authored_armor']:
            raise RuntimeError('Restored NPC did not take actual opposing fire')
        if real(row[4])>0 and (row[2] or row[3]):
            raise RuntimeError('Survivor retained the dead reactive target')
    if sum(real(r[4])<=0 for r in live)!=1:raise RuntimeError(f'Expected exactly one combat death: {live}')
    early=loaded['probe'];frame=early['rf_scene_profile_stage'][0]
    if not PROBE_FRAME<=frame<LOAD_FRAMES:raise RuntimeError('Missed post-combat observation')
    combat=x['rf_scene_enemy_combat']
    if combat[2]<2 or combat[3]<2 or early['rf_scene_enemy_combat'][2:4]!=combat[2:4]:
        raise RuntimeError('Real continued shots/hits absent or shots continued after target death')
    if real(combat[5])!=100:raise RuntimeError('Player became the restored target')
    return dict(result='PASS',saved_rows=rows,restored=restored,final_pair=live,
                shots=combat[2],hits=combat[3],free_pages=min(source['free_pages'],loaded['free_pages']),
                limitations='One fresh-boot two-NPC reactive continuation; complete inventory is recorded from RFNC, restored equipped loaded ammo is directly compared. No visual/audio or broader tactics claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only,friendly_delay=1.0)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('npc-opposed-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,details=prepare_level(folder/'level',friendly_delay=1.0)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<I',FRIENDLY));(DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(SAVE_FRAMES*48))
        build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,360,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['save']=source;payload=(folder/'save/xbox-world.rfwc').read_bytes();report['saved_rows']=validate_source(source,payload,details)
        (DISC/'campaign-setup.bin').unlink();(DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(LOAD_FRAMES*48))
        build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,360,extra_symbols=SYMBOLS,allow_guest_error=True,probe=probe,probe_frame=PROBE_FRAME)
        report['load']=loaded;report.update(validate(source,loaded,payload,details))
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
