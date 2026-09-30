"""Xbox RFTU3 removed-base save and fresh-load check, without replaying removal.

Real Auto Turret base1994 generates its head in isolated CTF06. Remove_Object
runs at frame60, ordinary save at75, then a fresh process loads for12 frames.
No images, host input, campaign route, fake head record or damage injection.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
from xemu_auto_turret_remove import BASE, START, REMOVE, prepare_level, SYMBOLS as REMOVE_SYMBOLS
from xemu_auto_turret import U, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES, LOAD_FRAMES = 75, 12
SYMBOLS = dict(REMOVE_SYMBOLS, rf_scene_turret_generated_restore_probe=32,
               rf_scene_world_load_reject=3, rf_scene_npc_checkpoint_reject_state=6)


def component(payload, kind):
    if len(payload)<320 or payload[:4]!=b'RFWC':
        raise RuntimeError('Missing ordinary world checkpoint')
    actual, offset, size=struct.unpack_from('<III',payload,128+(kind-1)*12)
    if actual!=kind or offset<320 or offset+size>len(payload):
        raise RuntimeError(f'Invalid checkpoint component {kind}')
    return payload[offset:offset+size]


def saved_row(payload):
    block=component(payload,11)
    if len(block)!=240 or block[:4]!=b'RFTU' or struct.unpack_from('<III',block,4)!=(3,0,1):
        raise RuntimeError('Expected RFTU3: empty inner payload and one 224-byte generated row')
    row=block[16:];base,role=struct.unpack_from('<II',row);v=row[8:192]
    uid,cls=struct.unpack_from('<II',v);health,armor=struct.unpack_from('<ff',v,8)
    dead,flags,flags810=struct.unpack_from('<III',v,16)
    target,remaining,burst=struct.unpack_from('<III',v,40)
    combat_action=struct.unpack_from('<I',v,60)[0]
    basehealth,baseflags,basedead,dispatch,targetrole,headflags,reason,retired=struct.unpack_from('<f7I',row,192)
    position=list(struct.unpack_from('<3I',v,64));basis=list(struct.unpack_from('<9I',v,76))
    angles=list(struct.unpack_from('<3I',v,112))
    if (base,role,uid,dead,dispatch,targetrole,target,remaining,burst,reason,retired)!=(BASE,1,0xffffffff,0,0,0,0xffffffff,0,0,1,1):
        raise RuntimeError('Saved removed identity, target, cadence or retirement marker differs')
    if health!=60 or not math.isfinite(armor) or armor<0 or flags810&1 or flags&(2|0x4000|0x100)!=(2|0x4000|0x100):
        raise RuntimeError('Removed head lost its nonlethal hidden tombstone state')
    if basehealth!=0 or baseflags&1 or not basedead&1 or basedead&0x10 or not headflags&0x100000:
        raise RuntimeError('Removed base readiness/vitals or generated-head flags invalid')
    if not all(math.isfinite(f(w)) for w in position+basis+angles):raise RuntimeError('Nonfinite saved head pose')
    npc=component(payload,2)
    if len(npc)<664 or npc[:4]!=b'RFNC' or struct.unpack_from('<I',npc,4)[0]!=10 or struct.unpack_from('<I',npc,16)[0]!=1:
        raise RuntimeError('Expected one RFNC10 authored base row')
    n=npc[64:];nuid,nclass,nretired=struct.unpack_from('<III',n)
    nhealth=struct.unpack_from('<f',n,20)[0]
    animation,deadpose,move,combat,shots=(struct.unpack_from('<I',n,o)[0] for o in (540,552,564,568,588))
    if (nuid,nretired,nhealth,deadpose)!=(BASE,1,0,0) or len(n)!=600+animation+move+combat+24*shots:
        raise RuntimeError('RFNC does not preserve the real retired base without a synthetic death pose')
    return dict(base_uid=base,role=role,head_uid=uid,class_index=cls,health=health,armor=armor,
        dead=dead,flags=flags,base_health=basehealth,removal_reason=reason,base_retired=retired,
        target_uid=target,remaining=remaining,burst=burst,combat_action=combat_action,
        death_dispatched=dispatch,position_words=position,basis_words=basis,angle_words=angles,
        npc_class_index=nclass,npc_retired=nretired,npc_dead_pose=deadpose,row_bytes=224)


def check_run(result, frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    if result['player_life'][2]:raise RuntimeError('Player died during bounded removed-pair save')
    x=result['extra'];stats=x['rf_scene_turret_generated_stats'];owners=x['rf_scene_turret_owners']
    if stats[0]!=1 or any(stats[5:]) or owners[0]!=1 or owners[2] or owners[3] or owners[7]:
        raise RuntimeError('Generated owner duplicate/death/damage/error detected')
    for name in ('rf_scene_turret_generated_death_probe','rf_scene_turret_death_effects','rf_scene_script_slays','rf_scene_turret_test'):
        if any(x[name]):raise RuntimeError(f'Unexpected death/effect/synthetic contact: {name}')
    if x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7] or x['rf_scene_turret_combat'][9] or x['rf_scene_turret_shots'][7] or x['rf_scene_actor_retirement'][3]:
        raise RuntimeError('Unexpected base handheld fire or runtime failure')
    return x


def check_tombstone(probe, base_present, shots):
    if probe[:3]!=[BASE,base_present,1] or probe[3]&(2|0x4000)!=(2|0x4000) or probe[4]!=0xffffffff or f(probe[5])!=60 or probe[6] or probe[7]!=shots:
        raise RuntimeError(f'Removed head registration, hidden/link/vitals/shot state differs: {probe}')


def validate_source(saved,payload):
    x=check_run(saved,SAVE_FRAMES);row=saved_row(payload);state=saved['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Ordinary source save failed: {state}')
    if any(x['rf_scene_turret_generated_restore_probe']):raise RuntimeError('Source unexpectedly restored a checkpoint')
    if x['rf_scene_turret_retirement']!=[1,0,0,BASE,1,0] or x['rf_scene_actor_retirement'][1]!=1 or x['rf_scene_setup_result']!=[2,REMOVE,2,0]:
        raise RuntimeError('Remove_Object did not complete exactly once through ordinary actor retirement')
    shots=x['rf_scene_turret_shots'][0]
    if not shots or not x['rf_scene_turret_shots'][1] or x['rf_scene_turret_combat'][3]!=shots:
        raise RuntimeError('Source head did not fire normally before retirement')
    check_tombstone(x['rf_scene_turret_retirement_probe'],1,shots)
    return row


def validate(saved,loaded,payload):
    row=validate_source(saved,payload);x=check_run(loaded,LOAD_FRAMES);state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary removed-pair restore failed: {state}')
    if any(x['rf_scene_setup_result']) or any(x['rf_scene_turret_retirement']):
        raise RuntimeError('Fresh load replayed scripted removal instead of restoring the marker')
    check_tombstone(x['rf_scene_turret_retirement_probe'],0,0)
    r=x['rf_scene_turret_generated_restore_probe']
    if r[:3]!=[1,BASE,1] or r[3]==r[4] or 0xffffffff in r[3:5] or r[5] or f(r[6])!=row['health'] or f(r[7])!=row['armor']:
        raise RuntimeError(f'Generated restore identity/vitals differs: {r}')
    if r[8:16]!=[0,0xffffffff,0xffffffff,0,0,0,0,row['combat_action']]:
        raise RuntimeError(f'Retired head restored target/cadence: {r}')
    if r[16:25]!=row['basis_words'] or r[25:28]!=row['angle_words'] or r[28:31]!=row['position_words'] or r[31]:
        raise RuntimeError('Restored generated pose or death-dispatch state differs from the saved wire row')
    if any(x['rf_scene_turret_shots']) or x['rf_scene_turret_combat'][1] or x['rf_scene_turret_combat'][3]:
        raise RuntimeError('Restored removed head acquired a target or fired')
    if x['rf_scene_turret_draw'][1] or x['rf_scene_turret_draw'][2]:raise RuntimeError('Removed head selected a death model or draw failed')
    return dict(result='PASS',saved_generated=row,restored_probe=r,
        restored_retirement_probe=x['rf_scene_turret_retirement_probe'],
        source_shots=saved['extra']['rf_scene_turret_shots'][0],loaded_shots=0,
        removal_replayed=False,base_unregistered=True,head_hidden_registered_tombstone=True,
        death_dispatches=0,death_effects=0,free_pages=min(saved['free_pages'],loaded['free_pages']),
        limitations='One removed-base RFTU3 save/fresh-load; head-only removal and visual/audio output remain unverified.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing',type=Path,help='Validate retained evidence only; no build/emulator')
    args=parser.parse_args()
    if args.validate_existing:
        folder=args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):raise RuntimeError('Disc restoration not confirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()),
            json.loads((folder/'load/result.json').read_text()),(folder/'save/xbox-world.rfwc').read_bytes()),indent=2));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-removed-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin','campaign-setup.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report=dict(result='FAIL',scope=__doc__,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(START,REMOVE))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS)
        report['phases']['save']=saved;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_generated']=validate_source(saved,payload)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'campaign-setup.bin').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
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
