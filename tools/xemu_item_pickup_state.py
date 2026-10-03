"""Bounded Xbox authored First Aid pickup OFF/ON and ordinary collection.

Original L7S1 Item_Pickup_State4035, Switch4036 and linked kits4037/4038 in
empty CTF06. Item positions alone change. Ordinary Heal(-25) creates a deficit;
Invert sends OFF, then the original delayed Switch enables collection. Neutral
gravity approach, no inventory/health write, images, host input or route.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import struct
import pefile

from build_fragment_platform_fixture import read_entry,U
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from inspect_events import inspect as inspect_events
from xemu_fusion_world_pickup import item_rows
from xemu_turret_combat import f
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

STATE,SWITCH,ITEM,DISTANT=4035,4036,4037,4038
SETUP,DEFICIT,OFF=914800,914801,914802
FRAMES=140
SYMBOLS={'rf_scene_item_pickup_state':8,'rf_scene_item_pickup_gate':8,
         'rf_scene_pickups':8,'rf_scene_pickup_vitals':4,
         'rf_scene_pickup_audio':10,'rf_scene_script_grants':8,
         'rf_scene_setup_result':4,'rf_scene_combat':8}


def originals():
    source=read_entry(ROOT/'Installed_Game/levels2.vpp','L7S1.rfl')
    meta=inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='L7S1.rfl'))
    items=[next(r for r in item_rows(source,meta) if r['uid']==uid) for uid in (ITEM,DISTANT)]
    if any(r['name']!='First Aid Kit' or r['quantity']!=25 for r in items):
        raise RuntimeError('Original linked First Aid Kits differ')
    sec=next(s for s in meta['sections'] if s['type']=='0x600')
    data=source[sec['offset']+8:sec['offset']+8+sec['size']]
    pe=pefile.PE(str(ROOT/'Installed_Game/RF.exe'));image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    types=[]
    for address_ in range(0x5a1a3c,0x5a1ba4,4):
        at=struct.unpack_from('<I',image,address_-base)[0]-base
        types.append(image[at:image.index(b'\0',at)].decode('cp1252'))
    events=inspect_events(data,types)
    state=next(r for r in events if r['uid']==STATE);switch=next(r for r in events if r['uid']==SWITCH)
    if state['type_index']!=54 or state['links']!=[ITEM,DISTANT] or state['delay']!=0:
        raise RuntimeError('Original item-state event differs')
    if switch['type_index']!=32 or switch['links']!=[STATE] or switch['words']!=[1,0] or switch['flags']!=[1,0] or abs(switch['delay']-.4)>.000001:
        raise RuntimeError('Original delayed enabling Switch differs')
    raw_events=[data[r['offset']:r['offset']+r['bytes']] for r in (state,switch)]
    return items,(state,switch),raw_events


def prepare_level(folder):
    items,authored,events_raw=originals()
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    spawn=next(s for s in meta['sections'] if s['type']=='0x70000')
    xyz=struct.unpack_from('<3f',original,spawn['offset']+8)
    positions=[(xyz[0],-.75,xyz[2]-.5),(xyz[0]+8,-.75,xyz[2]-.5)]
    copied=[]
    for row,position in zip(items,positions):
        value=bytearray(row['raw']);at=row['position'];struct.pack_into('<3f',value,at,*position)
        if value[:at]!=row['raw'][:at] or value[at+12:]!=row['raw'][at+12:]:
            raise RuntimeError('Original item modified beyond position')
        copied.append(value)
    deficit=bytearray(event(DEFICIT,'Heal','pickup_health_deficit',(),flags=(1,0)))
    at=4+2+len('Heal')+12+2+len('pickup_health_deficit')+1+4+2
    struct.pack_into('<i',deficit,at,-25)
    events=U(5)+event(SETUP,'Delay','pickup_state_setup',(DEFICIT,OFF))+deficit+\
        event(OFF,'Invert','pickup_off',(STATE,))+b''.join(events_raw)
    replacement={0x40000:U(2)+b''.join(copied),0x30000:U(0),0x600:events,0x60000:U(0)}
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    present={int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0:
            for missing in sorted(replacement.keys()-present):
                payload=replacement[missing];offsets[missing]=len(out);out+=U(missing,len(payload))+payload;added+=1
        payload=replacement.get(kind,original[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000]);struct.pack_into('<I',out,20,meta['declared_sections']+added)
    check=inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in item_rows(out,check)]!=[bytes(r) for r in copied]:
        raise RuntimeError('Item fixture failed exact round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,frames=FRAMES,source='levels2.vpp/L7S1.rfl',
                authored_events=authored,event_sha256=[hashlib.sha256(r).hexdigest() for r in events_raw],
                items=[dict(uid=r['uid'],quantity=r['quantity'],position=p,source_sha256=hashlib.sha256(r['raw']).hexdigest()) for r,p in zip(items,positions)],
                staged_fields=['item positions','ordinary deficit/Invert setup'],
                setup_frames={str(SETUP):0,str(SWITCH):60},expected_on_frame=84,probe_frame=70,
                limitations='Two authored item-state links; one ordinary First Aid collection. No player locomotion input, item visibility, saves, respawn or audiovisual-output claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def replay():return b'RFI6'+U(48)+bytes(FRAMES*48)


def live_probe(monitor,mapping):
    result={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    result['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return result


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    before,end=result['probe'],result['extra']
    if not 70<=before['frame']<83:raise RuntimeError('Missed disabled proximity window')
    state,gate=before['rf_scene_item_pickup_state'],before['rf_scene_item_pickup_gate']
    if state!=[2,0,2,0,DISTANT,0,2,0]:
        raise RuntimeError(f'OFF did not disable both original linked items: {state}')
    if not gate[0] or gate[1]!=ITEM or not 0<gate[2]<84 or gate[3:7]!=[2,0,0,0]:
        raise RuntimeError(f'No actual blocked nearby pickup attempt while disabled: {gate}')
    if before['rf_scene_pickups'][3:5]!=[0,0] or f(before['rf_scene_pickup_vitals'][0])!=75 or before['rf_scene_pickup_vitals'][2]:
        raise RuntimeError('Disabled nearby First Aid was collected or health deficit absent')
    state=end['rf_scene_item_pickup_state'];pickups=end['rf_scene_pickups'];vitals=end['rf_scene_pickup_vitals']
    if state!=[4,2,2,0,DISTANT,1,1,0]:
        raise RuntimeError(f'Original delayed Switch did not reenable both items: {state}')
    if pickups[3:6]!=[1,0,ITEM] or pickups[7] or f(vitals[0])!=100 or f(vitals[2])!=25:
        raise RuntimeError(f'Enabled ordinary First Aid collection missing or duplicated: {pickups}, {vitals}')
    if end['rf_scene_item_pickup_gate'][0]<gate[0] or end['rf_scene_item_pickup_gate'][2]>=84:
        raise RuntimeError('Disabled gate remained active after ON or lost retained evidence')
    audio=end['rf_scene_pickup_audio']
    if audio[0:5]!=[1,1,0,0,ITEM]:raise RuntimeError('Accepted pickup notification did not occur exactly once')
    for sample in (before,end):
        if any(sample['rf_scene_script_grants']) or sample['rf_scene_combat'][0]:
            raise RuntimeError('Scripted item grant or player weapon fire contaminated pickup flow')
    if end['rf_scene_setup_result']!=[2,SWITCH,32,0]:
        raise RuntimeError('Original Switch request failed')
    return dict(result='PASS',disabled_nearby_attempts=gate[0],disabled_health=75,
                collected_uid=ITEM,collections=pickups[3],health_restored=f(vitals[2]),
                final_health=f(vitals[0]),state_calls=state[0],free_pages=result['free_pages'],
                limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('item-pickup-state-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(SETUP,SWITCH));(DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'pickup')
        result=run_guest(folder,'pickup',hdd,FRAMES,360,snapshot=True,extra_symbols=SYMBOLS,probe=live_probe,probe_frame=70,allow_guest_error=True)
        report['native']=result;report.update(validate(result,recipe))
    except Exception as error:report['error']=str(error);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as error:report['result']='FAIL';report['restore_error']=str(error);raise
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
