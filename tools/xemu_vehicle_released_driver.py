"""Xbox former Jeep-driver release, owner switch and ordinary save continuation.

Original miner7646 retains its authored Jeep7629 seat link in the grounded
two-Jeep CTF06 fixture. Board beside the living driver, dispatch ordinary Slay,
exit and board B. Save B occupied; fresh load retains the inactive A relation,
then ordinary exit/Use returns to A without reviving or reseating the miner.
No host AI-mode/route links, fabricated ownership, host input or images.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U,F
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_npc_jeep_seat import ACTOR,jeep_seat
from xemu_npc_turret_seat import record_details
from xemu_npc_jeep_detached_save import component
from xemu_turret_combat import entity_rows,f
from xemu_vehicle_switch import A,B,prepare_level as prepare_pair
from xemu_vehicle_switch_save import SYMBOLS as SAVE_SYMBOLS
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SLAY=915220
SAVE_FRAMES,LOAD_FRAMES=350,140
SYMBOLS=dict(SAVE_SYMBOLS,rf_scene_script_slays=6,rf_scene_npc_seat_probe=12,
    rf_scene_npc_jeep_detached_probe=16,rf_scene_npc_seat_checkpoint=6,
    rf_scene_live_corpses=8,rf_scene_corpse_checkpoint=8,
    rf_scene_live_death_audio=4,rf_scene_jeep_npc_gunner=16)


def replay(load=False):
    uses=(60,100) if load else (90,210,250)
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(frame in uses),int(not load and 290<=frame<298),0,
        int(not load and frame==270),0) for frame in range(LOAD_FRAMES if load else SAVE_FRAMES))


def prepare_level(folder):
    path,recipe=prepare_pair(folder);raw=read_entry(path,'ctf06.rfl')
    source=read_entry(ROOT/'Installed_Game/levels2.vpp','L12S1.rfl')
    miner=next(row for row in entity_rows(source) if row['uid']==ACTOR)
    details=record_details(miner);seat=jeep_seat()
    if miner['name'].lower()!='miner1' or details['seat_host_uid']!=A:
        raise RuntimeError('Original miner does not retain the expected Jeep driver link')
    actor=bytearray(miner['raw']);at=miner['transform'];host=recipe['positions'][0]
    actor[at:at+48]=F(*(host[i]+seat['position'][i] for i in range(3)),0,0,1,1,0,0,0,1,0)
    if actor[:at]!=miner['raw'][:at] or actor[at+48:]!=miner['raw'][at+48:]:
        raise RuntimeError('Changed non-transform authored driver data')
    records=[r['raw'] for r in entity_rows(raw)]+[bytes(actor)]
    events=U(1)+command(SLAY,'Slay_Object','release_original_driver',(ACTOR,),delay=2.0)
    replacements={0x30000:U(3)+b''.join(records),0x600:events}
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=replacements.get(kind,raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=records:raise RuntimeError('Entity records failed roundtrip')
    archive(path,[('ctf06.rfl',out)])
    recipe.update(scope=__doc__,frames=SAVE_FRAMES,load_frames=LOAD_FRAMES,seat=seat,
        driver=details,setup_event=SLAY,
        replay={'board_A_beside_driver':90,'probe_living_seat':100,'slay_driver':120,
                'exit_A':210,'board_B':250,'gunner_B':270,'fire_B':[290,297],'save_B':350},
        load_replay={'probe_occupied_B':40,'exit_B':60,'return_A':100},
        limitations='One original released driver, parked same-class handoff and ordinary fresh-boot save; no live passenger handoff, route, moving host, current-session load or audiovisual verification.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay());(folder/'load-replay.bin').write_bytes(replay(True))
    return path,recipe


def live_probe(monitor,mapping):
    values={n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}
    values['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return values


def saved_rows(payload,recipe):
    block=component(payload,11)
    if block[:4]!=b'RFSW' or len(block)<344:raise RuntimeError('Missing switched-owner checkpoint')
    ver,inner,uid,profile,count=struct.unpack_from('<5I',block,4)
    if (ver,uid,profile,count)!=(1,B,3,1) or len(block)!=24+inner+320:raise RuntimeError('Invalid RFSW1 owner contract')
    bank=list(struct.unpack('<80I',block[24+inner:]));block=block[24:24+inner]
    corpse_lifetime=None
    if block[:4]==b'RFCL':
        ver,inner,count=struct.unpack_from('<3I',block,4)
        if (ver,count)!=(1,1) or len(block)!=16+inner+48:
            raise RuntimeError('Invalid single corpse lifetime companion')
        corpse_lifetime=list(struct.unpack_from('<12I',block,16+inner))
        if corpse_lifetime[0]!=ACTOR:raise RuntimeError('Corpse lifetime names another actor')
        block=block[16:16+inner]
    if block[:4]!=b'RFNS':raise RuntimeError('Missing retained authored seat relation')
    ver,inner,count=struct.unpack_from('<3I',block,4)
    if (ver,count)!=(1,1) or len(block)!=16+inner+24:raise RuntimeError('Invalid inactive RFNS1 relation')
    seat=list(struct.unpack_from('<6I',block,16+inner))
    if seat!=[ACTOR,A,recipe['seat']['index'],1,0,0]:raise RuntimeError(f'Saved seat is not inactive on original A: {seat}')
    block=block[16:16+inner]
    if block[:4]!=b'RFVA':raise RuntimeError('Missing parked A vehicle owner')
    ver,inner,count=struct.unpack_from('<3I',block,4)
    if (ver,count)!=(2,1) or len(block)!=16+inner+80 or inner!=160:raise RuntimeError('Unexpected RFVA2/RFVC3 vehicle structure')
    v=block[16:16+inner];p=list(struct.unpack('<20I',block[16+inner:]))
    if v[:4]!=b'RFVC' or struct.unpack_from('<II',v,4)!=(3,160) or struct.unpack_from('<II',v,16)!=(3,3) or struct.unpack_from('<I',v,108)[0]!=1:
        raise RuntimeError('Saved B is not an occupied live gunner Jeep')
    v=list(struct.unpack('<40I',v))
    if bank[:2]!=[A,1] or bank[3] or bank[8] or p[:2]!=[A,0] or p[19] or bank[16:28]!=p[2:14]:
        raise RuntimeError('Parked A bank lost living identity, role or exact pose')
    npc=component(payload,2)
    # The encoder defaults to10; only generated/reactive combat targets raise
    # the version to11/12. This dead, inactive driver has neither target mode.
    if npc[:4]!=b'RFNC' or struct.unpack_from('<I',npc,4)[0]!=10 or struct.unpack_from('<I',npc,16)[0]!=1 or len(npc)<664:
        raise RuntimeError('Expected RFNC10 single inactive original miner')
    row=npc[64:];uid,_,retired,flags=struct.unpack_from('<4I',row)
    health=struct.unpack_from('<f',row,20)[0];mode=struct.unpack_from('<i',row,524)[0]
    if uid!=ACTOR or retired or flags&0x4000 or not math.isfinite(health) or health>0 or mode==13 or struct.unpack_from('<I',row,552)[0]!=1:
        raise RuntimeError('Saved driver is not the actual dead detached actor')
    return dict(seat=seat,health=health,position=list(struct.unpack_from('<3f',row,28)),corpse_lifetime=corpse_lifetime,
        active_uid=B,active_ammo=v[26],active_rng=v[30],active_position=v[8:11],
        parked_uid=A,parked_ammo=bank[4],parked_rng=bank[6],parked_position=p[2:5])


def check_run(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    x=result['extra']
    if x['rf_scene_vehicle_state'][5] or x['rf_scene_vehicle_route_state'][0] or x['rf_scene_vehicle_route_state'][3] or x['rf_scene_vehicle_route_state'][7] or x['rf_scene_npc_seats'][9]:
        raise RuntimeError('Unexpected vehicle, seat or route error')
    if x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7]:raise RuntimeError('Unexpected NPC fire/error')


def detached(x,actor_handle,host_handle,passive=False):
    p=x['rf_scene_npc_jeep_detached_probe']
    if p[:4]!=[ACTOR,A,actor_handle,host_handle] or not math.isfinite(f(p[4])) or f(p[4])>0 or p[5]==13 or p[6]!=0xffffffff or p[9] or p[10]!=(0xffffffff if passive else 0) or p[15]!=1:
        raise RuntimeError(f'Former driver lost original identity or reacquired ownership: {p}')
    if x['rf_scene_npc_seats'][6] or x['rf_scene_npc_seats'][9]:raise RuntimeError('Former driver became attached again')
    return p


def validate_source(source,payload,recipe):
    check_run(source,SAVE_FRAMES);pre,x=source['probe'],source['extra'];state=source['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError(f'Source ordinary save failed: {state}')
    seat=pre['rf_scene_npc_seat_probe'];stats=pre['rf_scene_npc_seats']
    if not 100<=pre['frame']<120 or stats[:4]!=[1,1,0,0] or stats[5:]!=[0,1,ACTOR,A,0] or seat[:2]!=[ACTOR,A] or f(seat[4])<=0 or seat[6:8]!=[13,seat[3]]:
        raise RuntimeError('Original living driver was not physically attached before ordinary Slay')
    if pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_vehicle_state'][12]!=seat[3]:
        raise RuntimeError('Player did not board beside the original living driver')
    slay=x['rf_scene_script_slays']
    if x['rf_scene_setup_result']!=[1,SLAY,1,0] or slay[:3]!=[1,1,ACTOR] or f(slay[3])>0 or slay[5] or any(pre['rf_scene_script_slays']):
        raise RuntimeError('Ordinary driver death did not occur once after boarding')
    if x['rf_scene_npc_seats'][5:9]!=[1,0,ACTOR,A]:raise RuntimeError('Original driver did not release its seat exactly once')
    sw=x['rf_scene_vehicle_switch'];apply=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:8]!=[A,B,seat[3],x['rf_scene_vehicle_state'][12]] or sw[8] or sw[15]!=B or apply[29]!=1:
        raise RuntimeError(f'Released authored seat still blocked ordinary vehicle switching: {sw}')
    if x['rf_scene_vehicle_state'][1:4]!=[2,1,1] or x['rf_scene_jeep_seats'][4]!=1 or x['rf_scene_apc_primary'][1]<1 or x['rf_scene_apc_primary'][7]>=apply[9]:
        raise RuntimeError('Second vehicle failed normal boarding, gunner fire or ammunition debit')
    p=detached(x,seat[2],seat[3],passive=True);saved=saved_rows(payload,recipe)
    if p[7:9]!=[0xffffffff]*2 or saved['health']!=f(p[4]) or saved['active_ammo']!=x['rf_scene_apc_primary'][7] or saved['parked_ammo']!=apply[8]:
        raise RuntimeError('Saved inactive relation or real vehicle ammunition disagrees with runtime')
    if x['rf_scene_npc_seat_checkpoint'][0]!=1 or x['rf_scene_npc_seat_checkpoint'][5]:raise RuntimeError('Seat capture failed')
    return saved


def validate(source,loaded,payload,recipe):
    saved=validate_source(source,payload,recipe);check_run(loaded,LOAD_FRAMES)
    pre,x=loaded['probe'],loaded['extra'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']):raise RuntimeError('Fresh ordinary world load failed')
    restore=x['rf_scene_vehicle_switch_restore'];a_handle=x['rf_scene_vehicle_state'][12];b_handle=pre['rf_scene_vehicle_state'][12]
    if restore!=[1,B,3,1,1,A,saved['parked_ammo'],saved['parked_rng'],0,0,0,saved['active_ammo'],saved['active_rng'],b_handle,a_handle,0]:
        raise RuntimeError('Vehicle bank restoration lost ammunition, RNG or fresh owner identity')
    p=pre['rf_scene_npc_jeep_detached_probe'];detached(pre,p[2],a_handle,passive=True);detached(x,p[2],a_handle)
    if p[7:9]!=[0xffffffff]*2 or f(p[4])!=saved['health']:raise RuntimeError('Fresh-load passive A regained driver ownership')
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_apc_primary'][7]!=saved['active_ammo'] or pre['rf_scene_jeep_seats'][4]!=1:
        raise RuntimeError('Fresh load did not retain occupied B without reboarding')
    if x['rf_scene_npc_seat_checkpoint'][:4]!=[0,1,1,0] or x['rf_scene_npc_seat_checkpoint'][5]:raise RuntimeError('Inactive RFNS relation did not restore exactly once')
    sw=x['rf_scene_vehicle_switch'];apply=x['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:8]!=[B,A,b_handle,a_handle] or sw[8] or sw[9]!=1 or sw[15]!=A or apply[8:10]!=[saved['active_ammo'],saved['parked_ammo']] or apply[29]!=1 or not 99<=apply[30]<=101:
        raise RuntimeError('Ordinary return to saved former driver vehicle failed')
    if x['rf_scene_vehicle_state'][1:4]!=[1,1,1] or x['rf_scene_apc_primary'][7]!=saved['parked_ammo'] or x['rf_scene_apc_primary'][1]:raise RuntimeError('Return changed ammo or replayed firing')
    if any(x['rf_scene_script_slays']) or any(x['rf_scene_setup_result']) or any(x['rf_scene_live_death_audio']):raise RuntimeError('Load replayed setup, driver death or death audio')
    return dict(result='PASS',saved=saved,restore=restore,return_exchange=apply,
        restored_inactive_relation=p,limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--resume-saved-run',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-released-driver-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',phases={},recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SLAY));(DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve();source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes();report['resumed_source']=str(prior)
        else:
            build(folder,'save');source=run_guest(folder,'save',hdd,SAVE_FRAMES,600,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=100,allow_guest_error=True)
            report['phases']['save']=source;payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=source;report['saved']=validate_source(source,payload,recipe)
        (DISC/'campaign-setup.bin').unlink();(DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(True));build(folder,'load')
        loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=40,allow_guest_error=True)
        report['phases']['load']=loaded;report.update(validate(source,loaded,payload,recipe))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_error']=str(exc);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
