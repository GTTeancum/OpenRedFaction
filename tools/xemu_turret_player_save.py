"""One Xbox ordinary mounted stationary-turret save and fresh continuation.

Reuse the authored Stationary Turret control fixture: Use30, aim35..50, fire
45..70, settle, one shot118 then neutral119 before source120 save. The installed
Vauss0.08s cadence leaves a pending deadline. Freshload100 has no boarding input:
fire20..40, live probe60, ordinary Use80 exit. No images, host input, fabricated
inventory/damage or campaign traversal. Parent owns serial native execution.
"""
import argparse
import datetime
import json
import math
from pathlib import Path
import struct
import tomllib

from xemu_stationary_turret_player import prepare_control,UID,SYMBOLS as BASE_SYMBOLS,f,U
from xemu_npc_jeep_detached_save import component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES,LOAD_FRAMES=120,100
SYMBOLS=dict(BASE_SYMBOLS,rf_scene_turret_player_checkpoint=20,
             rf_scene_turret_checkpoint=6,rf_scene_world_load_reject=3,
             rf_scene_checkpoint_world_reject=9,rf_scene_setup_result=4)


def replay(load=False):
    count=LOAD_FRAMES if load else SAVE_FRAMES
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,
        .25 if not load and 35<=frame<=50 else 0,0,0,
        int(frame==(80 if load else 30)),int((20<=frame<=40) if load else (45<=frame<=70 or frame==118)),0,0,0)
        for frame in range(count))


def prepare_level(folder):
    path=prepare_control(folder)
    recipe=json.loads((folder/'recipe.json').read_text())
    recipe.update(scope=__doc__,source_frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,
                  use_frames=[30],load_use_frames=[80],fire_frames=[45,70],
                  last_source_shot=118,source_release=119,
                  load_fire_frames=[20,40],probe_frame=60,no_reboarding=True)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    (folder/'load-replay.bin').write_bytes(replay(True))
    return path


def saved_control(payload):
    block=component(payload,11)
    # Ordinary corpse lifetime is the optional outer companion; RFTP owns
    # only its nested mounted-player row and leaves the corpse bytes intact.
    if block[:4]==b'RFCL':
        if len(block)<16:raise RuntimeError('Truncated outer RFCL wrapper')
        version,inner,count=struct.unpack_from('<3I',block,4)
        if version!=1 or count>30 or len(block)!=16+inner+count*48:
            raise RuntimeError('Malformed outer RFCL1 wrapper')
        block=block[16:16+inner]
    if len(block)<80 or block[:4]!=b'RFTP':raise RuntimeError('Missing mounted-player RFTP wrapper')
    version,inner,size=struct.unpack_from('<3I',block,4)
    if version!=1 or size!=64 or len(block)!=80+inner:raise RuntimeError('Malformed RFTP1')
    row=list(struct.unpack_from('<16I',block,16+inner))
    if row[0]!=UID or any(row[14:]) or not 0<row[2]<=36000:raise RuntimeError('Wrong mounted owner/pending cadence/reserved words')
    if not all(math.isfinite(f(x)) for x in row[5:14]) or not any(abs(f(x))>.0001 for x in row[5:8]):
        raise RuntimeError('Source did not save actual nondefault aim')
    turret=block[16:16+inner]
    if len(turret)!=200 or turret[:4]!=b'RFTU' or struct.unpack_from('<3I',turret,4)!=(1,0,1):
        raise RuntimeError('Expected one ordinary authored RFTU1 owner')
    t=turret[16:]
    if struct.unpack_from('<2I',t)!=(UID,row[1]) or struct.unpack_from('<I',t,16)[0] or struct.unpack_from('<f',t,8)[0]!=200:
        raise RuntimeError('Saved turret is not the original living mount')
    if struct.unpack_from('<3I',t,40)!=(0xffffffff,0,0):
        raise RuntimeError('Autonomous turret target/cadence leaked into player ownership')
    player=component(payload,1)
    if len(player)!=544 or player[:4]!=b'RFPL' or struct.unpack_from('<I',player,4)[0] not in (1,2,4) or struct.unpack_from('<2I',player,8)!=(544,0):
        raise RuntimeError('Missing ordinary standing player body')
    return dict(uid=row[0],class_id=row[1],remaining=row[2],burst=row[3],rng=row[4],
                desired=row[5:8],anchor=row[8:11],eye_offset=row[11:14],
                actual_aim=list(struct.unpack_from('<3I',t,112)),
                player_position=list(struct.unpack_from('<3I',player,32)))


def probe(monitor,mapping):
    x={n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}
    x['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return x


def check_run(run,frames):
    if run['guest_phase']!=5 or run['frames']!=frames or run['memory_bytes']!=64*1024*1024 or run['free_pages']<=0 or run['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB turret control run')
    x=run['extra'];p=x['rf_scene_turret_player'];shots=x['rf_scene_turret_shots'];combat=x['rf_scene_turret_combat']
    if p[7] or shots[7] or combat[9] or combat[3] or x['rf_scene_turret_owners'][7]:
        raise RuntimeError('Control/turret error or autonomous shot dispatch')
    if shots[0]!=p[5] or not p[5] or x['rf_scene_combat'][0] or any(x['rf_scene_turret_test']) or any(x['rf_scene_setup_result']):
        raise RuntimeError('Missing mounted shots or unexpected handheld/fixture/setup path')


def check_mounted(x,entries,host=None,player=None):
    p=x['rf_scene_turret_player'];pose=x['rf_scene_turret_player_probe']
    host=p[9] if host is None else host;player=p[8] if player is None else player
    if host==player or 0xffffffff in (host,player) or p[0]!=entries or p[3]!=UID or p[8:10]!=[player,host]:
        raise RuntimeError('Mounted player/turret identity changed')
    if pose[:2]!=[1,UID] or pose[4:7]!=[1,1,host] or f(pose[2])!=200 or f(pose[3])!=100:
        raise RuntimeError(f'Actual occupancy, weapon owner or body vitals invalid: {pose}')
    return host,player


def validate_source(run,payload):
    check_run(run,SAVE_FRAMES);x=run['extra'];state=run['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Mounted source failed ordinary save: {state}')
    saved=saved_control(payload);p=x['rf_scene_turret_player'];cp=x['rf_scene_turret_player_checkpoint']
    # Normal scene close releases control AFTER saving. The last tick probe
    # still proves mounted occupancy; do not count teardown as a Use exit.
    check_mounted(x,1)
    if p[1:3]!=[1,0] or p[11]!=3 or cp[:2]!=[1,0] or not x['rf_scene_turret_combat'][2]:
        raise RuntimeError('Source did not aim/fire/save mounted before normal teardown')
    if x['rf_scene_turret_player_probe'][16:19]!=saved['anchor']:
        raise RuntimeError('RFTP changed the retained boarding anchor')
    return saved


def validate(source,loaded,payload):
    saved=validate_source(source,payload);check_run(loaded,LOAD_FRAMES)
    x=loaded['extra'];live=loaded['probe'];cp=x['rf_scene_turret_player_checkpoint'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Mounted ordinary fresh load failed: {state}')
    if cp[:3]!=[0,1,UID] or cp[5:8]!=[saved['remaining'],saved['burst'],saved['rng']]:
        raise RuntimeError(f'RFTP did not restore cadence/shared RNG exactly once: {cp}')
    if cp[8:]!=saved['desired']+saved['anchor']+saved['eye_offset']+saved['actual_aim']:
        raise RuntimeError('Immutable restore changed desired/actual aim, body anchor or eye offset')
    if not 60<=live['frame']<80:raise RuntimeError('Missed restored mounted pre-exit observation')
    check_mounted(live,0,cp[3],cp[4]);p=live['rf_scene_turret_player'];pose=live['rf_scene_turret_player_probe']
    if p[1:3]!=[0,1] or p[5]<1 or pose[16:19]!=saved['anchor']:
        raise RuntimeError('Loaded player did not fire while mounted without reboarding')
    if max(abs(f(pose[8+i])-f(saved['player_position'][i])) for i in (0,2))>.05:
        raise RuntimeError('Restore displaced the standing player body horizontally')
    end=x['rf_scene_turret_player'];last=x['rf_scene_turret_player_probe']
    if end[:4]!=[0,1,0,UID] or end[11] or end[5]!=p[5] or last[:2]!=[0,UID] or last[4:7]!=[0,0,0xffffffff]:
        raise RuntimeError('Normal Use exit failed or firing continued after release/exit')
    return dict(result='PASS',saved=saved,restored=cp,source_shots=source['extra']['rf_scene_turret_player'][5],
                resumed_shots=end[5],fresh_boot_without_reboarding=True,normal_exit=True,
                limitations='One released-input authored stationary-turret control save. Exact desired/actual aim, cadence, RNG, anchor and eye offset restore checked; no images, target damage, held-input saves or in-session reload checked.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group();mode.add_argument('--prepare-only',type=Path)
    mode.add_argument('--resume-saved-run',type=Path,help='Reuse completed source payload/private HDD; run only fresh load and preserve original report')
    args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT)
    source=payload=None
    if args.resume_saved_run:
        saved_folder=args.resume_saved_run.resolve()
        source=json.loads((saved_folder/'save/result.json').read_text())
        payload=(saved_folder/'save/xbox-world.rfwc').read_bytes()
        validate_source(source,payload)
        saved_config=tomllib.loads((saved_folder/'save/xemu.toml').read_text())
        saved_hdd=Path(saved_config['sys']['files']['hdd_path'])
        if not saved_hdd.is_file():raise RuntimeError('Original private save HDD is missing; source will not rerun')
        hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
        if hdd.resolve()!=saved_hdd.resolve():raise RuntimeError('Original save belongs to another HDD')
        fixture=saved_folder/'level/scene-fixture.vpp'
        if not fixture.is_file():raise RuntimeError('Original source fixture is missing')
        folder=saved_folder/('resume-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
        folder.mkdir()
    else:
        hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
        folder=ROOT/'artifacts/xemu'/('turret-player-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
        fixture=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__)
    if args.resume_saved_run:report.update(reused_source=str(saved_folder),save=source)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        if not args.resume_saved_run:
            (DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,360,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
            report['save']=source;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved']=validate_source(source,payload)
        (DISC/'world-hdd-save.flag').unlink(missing_ok=True);(DISC/'world-hdd-load.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay(True))
        build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,360,extra_symbols=SYMBOLS,allow_guest_error=True,probe=probe,probe_frame=60)
        report['load']=loaded;report.update(validate(source,loaded,payload))
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
