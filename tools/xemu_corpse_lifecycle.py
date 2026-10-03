"""Bounded stock64MiB corpse retention/fade/resource-release check.

Six complete copied miner8432 records, explicit fixture UIDs/transforms and one
ordinary Slay event in empty CTF06. No campaign route, host input or images.
Parent runs serial Xbox builds/XEMU; --prepare-only reads/writes fixture assets.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry, U, F
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTORS = tuple(range(913100, 913106))
SLAY, FRAMES, PROBE_FRAME = 913110, 120, 20
SYMBOLS = {'rf_scene_live_corpses':8, 'rf_scene_corpse_lifecycle':9,
           'rf_scene_setup_result':4, 'rf_scene_script_slays':6,
           'rf_scene_profile_stage':4, 'rf_scene_npc_models':4,
           'rf_scene_corpse_source_retirement':12,
           'campaign_model_owned_count':1, 'campaign_model_owned_bytes':1,
           'campaign_live_corpse_count':1, 'campaign_live_corpse_object_count':1}


def prepare_level(folder,slay_delay=0):
    source = read_entry(ROOT/'Installed_Game/levels1.vpp','L1S1.rfl')
    miner = next(r for r in entity_rows(source) if r['uid'] == 8432)
    if miner['name'].lower() != 'miner1':raise RuntimeError('Expected authored miner8432 class')
    raw = read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(raw):raise RuntimeError('CTF06 is no longer an empty actor testbed')
    meta = inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    start = next(s for s in meta['sections'] if s['type'] == '0x70000')
    spawn = struct.unpack_from('<3f',raw,start['offset']+8)
    copies, positions = [], []
    for i,uid in enumerate(ACTORS):
        copied = bytearray(miner['raw']);struct.pack_into('<I',copied,0,uid)
        position = (spawn[0]+(i%3-1)*2,spawn[1],spawn[2]-4-(i//3)*3)
        at = miner['transform'];copied[at:at+48] = F(*position,0,0,1,1,0,0,0,1,0)
        copies.append(copied);positions.append(position)
    entity = U(len(copies))+b''.join(copies)
    slay = bytearray(event(SLAY,'Slay_Object','corpse_lifecycle_slay',ACTORS))
    delay_at = 4+2+len('Slay_Object')+12+2+len('corpse_lifecycle_slay')+1
    struct.pack_into('<f',slay,delay_at,slay_delay)
    events = U(1)+slay
    out = bytearray(raw[:meta['sections'][0]['offset']]);offsets = {};added = 0
    present = {int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind = int(section['type'],16)
        if kind == 0:
            for missing,payload in ((0x30000,entity),(0x600,events)):
                if missing not in present:
                    offsets[missing] = len(out);out += U(missing,len(payload))+payload;added += 1
        payload = raw[section['offset']+8:section['offset']+8+section['size']]
        if kind == 0x30000:payload = entity
        elif kind == 0x600:payload = events
        elif kind == 0x60000:payload = U(0)
        offsets[kind] = len(out);out += U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [(r['uid'],r['name']) for r in entity_rows(out)] != [(u,miner['name']) for u in ACTORS]:
        raise RuntimeError('Fixture actor ordering/identity changed')
    folder.mkdir(parents=True,exist_ok=True);path = folder/'scene-fixture.vpp'
    archive(path,[('ctf06.rfl',out)])
    (folder/'recipe.json').write_text(json.dumps(dict(scope=__doc__,source_uid=8432,
        source_class=miner['name'],source_record_sha256=hashlib.sha256(miner['raw']).hexdigest(),
        fixture_uids=ACTORS,positions=positions,changed_fields=['UID','transform'],
        event=dict(uid=SLAY,type='Slay_Object',links=ACTORS,request_frame=0,
                   delay_seconds=slay_delay,frame=round(slay_delay*60))),indent=2)+'\n')
    return path


def during_fade(monitor,mapping):
    result = {n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}
    # Existing x86 pool prefix:30 next-slot indices, free_head/live/peak/mask.
    result['pool'] = words(monitor,address(mapping,'campaign_live_corpses'),34)[30:34]
    owner_pointer = words(monitor,address(mapping,'campaign_model_owners'),1)[0]
    owner_count = words(monitor,address(mapping,'campaign_model_owner_count'),1)[0]
    if not owner_pointer or owner_count != 6:raise RuntimeError('Expected exactly six fixture model owners')
    result['loaded_models'] = [words(monitor,owner_pointer+i*80,1)[0] for i in range(6)]
    return result


def saved_retired_actor(payload):
    if len(payload)<320 or payload[:4]!=b'RFWC':raise RuntimeError('Missing lifecycle world save')
    kind,offset,size=struct.unpack_from('<III',payload,140)
    if kind!=2 or offset+size>len(payload):raise RuntimeError('Invalid NPC component bounds')
    blob=payload[offset:offset+size]
    # RFNC11 changes the generated-head Attack target discriminator only;
    # both versions retain the600-byte row and identical variable-tail spans.
    if len(blob)<64 or blob[:4]!=b'RFNC' or struct.unpack_from('<I',blob,4)[0] not in (10,11):
        raise RuntimeError('Expected RFNC10/11 lifecycle save')
    count=struct.unpack_from('<I',blob,16)[0];at=64;retired=[];deaths=[]
    for _ in range(count):
        if at+600>len(blob):raise RuntimeError('Truncated lifecycle NPC row')
        uid,_,removed=struct.unpack_from('<III',blob,at)
        health=struct.unpack_from('<f',blob,at+20)[0]
        animation=struct.unpack_from('<I',blob,at+540)[0]
        dead=struct.unpack_from('<I',blob,at+552)[0]
        move,combat=struct.unpack_from('<II',blob,at+564);shots=struct.unpack_from('<I',blob,at+588)[0]
        if uid not in ACTORS or health>0:raise RuntimeError('Unexpected living lifecycle actor')
        if removed:
            if removed!=1 or dead or animation:raise RuntimeError('Retired actor retained a nonexistent pose')
            retired.append(uid)
        else:
            if dead!=1 or animation!=120:raise RuntimeError('Remaining corpse lost its settled pose')
            deaths.append(uid)
        at+=600+move+combat+24*shots+animation
    if at!=len(blob) or retired!=[ACTORS[0]] or deaths!=list(ACTORS[1:]):
        raise RuntimeError(f'Expired corpse retirement identity mismatch: {retired}, {deaths}')
    return dict(retired_uid=retired[0],retained_death_uids=deaths)


def validate(result,payload):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or \
       result['memory_bytes'] != 64*1024*1024 or result['free_pages'] <= 0 or result['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB lifecycle run')
    live, final = result['probe'], result['extra']
    frame = live['rf_scene_profile_stage'][0]
    if not PROBE_FRAME <= frame < 55:raise RuntimeError(f'Missed early fade observation: frame{frame}')
    for x in (live,final):
        if x['rf_scene_setup_result'] != [1,SLAY,1,0] or x['rf_scene_script_slays'][0:2] != [6,6] or x['rf_scene_script_slays'][5]:
            raise RuntimeError('Ordinary Slay did not create six deaths')
        if x['rf_scene_live_corpses'][0] != 6 or any(x['rf_scene_live_corpses'][6:]) or x['rf_scene_corpse_lifecycle'][5]:
            raise RuntimeError('Corpse creation/fade error or unsupported fallback')
    if live['rf_scene_live_corpses'][1] != 6 or live['rf_scene_live_corpses'][5] or \
       live['campaign_model_owned_count'] != [6] or live['campaign_live_corpse_count'] != [6] or \
       live['campaign_live_corpse_object_count'] != [6] or live['pool'][1:] != [6,6,63] or \
       any(v != 1 for v in live['loaded_models']):
        raise RuntimeError('Corpse/model ownership was not retained through the fade interval')
    fading = live['rf_scene_corpse_lifecycle']
    if not fading[1] or not fading[2] or fading[6] != ACTORS[0] or not 0 < fading[7] < fading[8] < 255:
        raise RuntimeError(f'Oldest corpse did not submit decreasing nonopaque geometry: {fading}')
    # The completed endpoint has closed the level. Its last gameplay tick and
    # immutable save retain the five bodies; teardown retires their resources.
    if final['rf_scene_live_corpses'][1] != 5 or final['rf_scene_live_corpses'][5] != 1 or \
       final['rf_scene_npc_models'][2:] != [6,0]:
        raise RuntimeError('Expired corpse did not release exactly one model/list owner')
    if any(final[n][0] for n in ('campaign_model_owned_count','campaign_model_owned_bytes',
                                'campaign_live_corpse_count','campaign_live_corpse_object_count')):
        raise RuntimeError('Level teardown retained corpse/model resources')
    final_fade = final['rf_scene_corpse_lifecycle']
    if not final_fade[2] > fading[2] or not final_fade[7] < fading[7]:
        raise RuntimeError('Fade did not continue before retirement')
    retired=final['rf_scene_corpse_source_retirement']
    if retired[:4]!=[1,0,ACTORS[0],0] or retired[9:]!=[0,0,1]:
        raise RuntimeError(f'Source actor/body/persistence retirement failed: {retired}')
    state=result['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):
        raise RuntimeError(f'Ordinary save failed after visual corpse retirement: {state}')
    saved=saved_retired_actor(payload)
    return dict(result='PASS',created=6,retained=5,retired=1,oldest_uid=ACTORS[0],
        fade_observed_frame=frame,fade_alpha_range=[final_fade[7],final_fade[8]],
        faded_vertices=final_fade[2],teardown_released_pose_bytes=live['campaign_model_owned_bytes'][0]-final['campaign_model_owned_bytes'][0],
        saved_actors=saved,free_pages=result['free_pages'],limitations='One oldest-body fade/retirement and ordinary save; protected bodies, reload, fade saves, pool saturation and visual appearance not checked.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare fixture only, without build/emulator')
    parser.add_argument('--validate-existing',type=Path,help='Validate an existing result.json only')
    args = parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    if args.validate_existing:
        print(json.dumps(validate(json.loads(args.validate_existing.read_text()),
              (args.validate_existing.parent/'xbox-world.rfwc').read_bytes()),indent=2));return
    require_no_project_xemu(ROOT);hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('corpse-lifecycle-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SLAY));(DISC/'campaign-actor.bin').write_bytes(U(ACTORS[0]))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'lifecycle')
        result = run_guest(folder,'lifecycle',hdd,FRAMES,360,snapshot=True,capture_world=True,
                           extra_symbols=SYMBOLS,probe=during_fade,probe_frame=PROBE_FRAME)
        report['run'] = result;report.update(validate(result,(folder/'lifecycle/xbox-world.rfwc').read_bytes()))
    except Exception as exc:report['error'] = str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:
            report['result']='FAIL';report['restore_build_error']=str(exc);raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None) == data for n,data in original.items())
            if not report['disc_restored']:report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__ == '__main__':main()
