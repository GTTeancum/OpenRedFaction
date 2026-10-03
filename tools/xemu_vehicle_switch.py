"""Bounded Xbox ordinary Use roundtrip between two parked unoccupied Jeeps.

Copies the complete original Jeep7629 twice into empty CTF06; second record
gets fixtureUID915200. Only UID/transforms and player spawn are staged. Real
Use/seat-cycle/fire/exit inputs spend distinct ammunition before switching
back. No NPC, scripted route, forced health/ammo, host input or images.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U,F
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_turret_combat import entity_rows,f
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

A,B=7629,915200
FRAMES=480
SYMBOLS={'rf_scene_vehicle_switch':16,'rf_scene_vehicle_switch_apply':32,
         'rf_scene_vehicle_switch_history':128,
         'rf_scene_vehicle_selection':12,'rf_scene_vehicle_state':16,
         'rf_scene_vehicle_damage':8,'rf_scene_jeep_seats':8,
         'rf_scene_apc_primary':8,'rf_scene_vehicle_route_state':8,
         'rf_scene_npc_seats':10,'rf_scene_enemy_combat':8,
         'rf_scene_setup_result':4}


def replay():
    # Mirrored Jeep bases make the normal positive-local-X exits face the
    # central gap; Use always selects by actual post-exit player position.
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(frame in (90,210,250,370,410)),int(130<=frame<150 or 290<=frame<298),
        0,int(frame in (110,270)),0) for frame in range(FRAMES))


def prepare_level(folder):
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('Expected actor-free CTF06')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    source=read_entry(ROOT/'Installed_Game/levels2.vpp','L12S1.rfl')
    jeep=next(r for r in entity_rows(source) if r['uid']==A)
    details=record_details(jeep)
    if jeep['name']!='Jeep01' or details['seat_host_uid']!=-1:raise RuntimeError('Original unparented Jeep differs')
    # Original wheel sphere low point=.040839687-.75. CTF06 floor171=-1.25.
    ground=-1.25-(.040839687-.75)+.01
    positions=[(-2.5,ground,1),(2.5,ground,1)]
    bases=[(0,0,1,1,0,0,0,1,0),(0,0,-1,-1,0,0,0,1,0)]
    records=[]
    for uid,position,basis in zip((A,B),positions,bases):
        value=bytearray(jeep['raw']);at=jeep['transform']
        struct.pack_into('<I',value,0,uid);value[at:at+48]=F(*position,*basis)
        if value[4:at]!=jeep['raw'][4:at] or value[at+48:]!=jeep['raw'][at+48:]:
            raise RuntimeError('Jeep modified beyond fixtureUID and transform')
        records.append(bytes(value))
    spawn=(-2.5,1.4513111,-1)
    replacements={0x30000:U(2)+b''.join(records),0x600:U(0),0x60000:U(0),0x40000:U(0),
                  0x70000:F(*spawn,*bases[0])}
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    present={int(s['type'],16) for s in meta['sections']}
    for section in meta['sections']:
        kind=int(section['type'],16)
        if kind==0:
            for missing in sorted(replacements.keys()-present):
                payload=replacements[missing];offsets[missing]=len(out);out+=U(missing,len(payload))+payload;added+=1
        payload=replacements.get(kind,original[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000]);struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=records:raise RuntimeError('Jeep record roundtrip failed')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,frames=FRAMES,source='levels2.vpp/L12S1.rfl',original_uid=A,fixture_uid=B,
                source_sha256=hashlib.sha256(jeep['raw']).hexdigest(),details=details,positions=positions,spawn=spawn,
                replay={'board_A':90,'gunner_A':110,'fire_A':[130,149],'probe_A':190,'exit_A':210,
                        'board_B':250,'gunner_B':270,'fire_B':[290,297],'exit_B':370,'return_A':410},
                geometry='Floor171 y=-1.25; hull lateral spheres extend ~1.41m, so5m centres leave2.18m between hulls. Roof sphere top~1.86m below ceiling3m. Actual exit/head clearance must still pass.',
                limitations='Two same-class parked Jeeps, unchanged authored vitals, real ammo debit. No NPC seats, cross-profile switch, routes, switched saves or audiovisual verification.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');(folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def live_probe(monitor,mapping):
    result={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    result['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return result


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    pre=result['probe'];end=result['extra']
    if not 190<=pre['frame']<210:raise RuntimeError('Missed first Jeep spent-ammo probe')
    if pre['rf_scene_vehicle_selection']!=[2,2,0,0,0,0,0,2,A,3,2,0]:raise RuntimeError('Wrong initial metadata-selected Jeep')
    if pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][1]<1:
        raise RuntimeError('First Jeep did not board, change to gunner and fire real rounds')
    if pre['rf_scene_vehicle_switch'][1]:raise RuntimeError('Unexpected switch before first Jeep probe')
    first_handle=pre['rf_scene_vehicle_state'][12];spent_ammo=pre['rf_scene_apc_primary'][7]
    state=end['rf_scene_vehicle_switch']
    if state[1]!=2 or state[4:8]!=[B,A,state[6],first_handle] or state[6] in (0,first_handle,0xffffffff) or state[8] or state[9]!=1 or state[11]!=spent_ammo or state[15]!=A:
        raise RuntimeError(f'Ordinary Use roundtrip did not preserve authored identities/ammo: {state}')
    if end['rf_scene_vehicle_state'][1:4]!=[3,2,1] or end['rf_scene_vehicle_state'][12]!=first_handle or end['rf_scene_apc_primary'][7]!=spent_ammo:
        raise RuntimeError('Roundtrip failed ownership or replenished first Jeep ammunition')
    if end['rf_scene_apc_primary'][1]<=pre['rf_scene_apc_primary'][1] or state[10]==spent_ammo:
        raise RuntimeError('Second Jeep did not retain its independently spent ammunition')
    apply=end['rf_scene_vehicle_switch_apply']
    history=end['rf_scene_vehicle_switch_history'];outbound=history[:32]
    if history[32:64]!=apply or any(history[64:]) or outbound[:4]!=[A,B,first_handle,state[6]] or outbound[8]!=spent_ammo or outbound[24] or outbound[29]!=1:
        raise RuntimeError('First/return commits did not preserve both authored handles and original ammo debit')
    if outbound[9]<=state[10] or outbound[9]<=spent_ammo:
        raise RuntimeError('Both parked owners must retain ammunition independently spent by real firing')
    # Ordinary Jeep exit returns its seat role to driver before parking.
    if apply[:4]!=[B,A,state[6],first_handle] or apply[8:10]!=[state[10],spent_ammo] or apply[24:30]!=[1,0,0,0,0,1] or not 409<=apply[30]<=411 or apply[31]:
        raise RuntimeError(f'Last exchange lost exact owner/role/ammo or registry coherence: {apply}')
    position=[f(v) for v in apply[15:18]];before=[f(v) for v in pre['rf_scene_vehicle_state'][6:9]]
    if not all(math.isfinite(v) for v in position+before) or max(abs(a-b) for a,b in zip(position,before))>.05 or any(apply[22:24]):
        raise RuntimeError('Parked first Jeep changed physical position or gained scripted freeze state')
    if outbound[12:15]!=apply[15:18] or outbound[4]!=apply[5] or outbound[6]!=apply[7]:
        raise RuntimeError('Returning to parked first Jeep changed its exact captured pose/vitals')
    if [f(v) for v in apply[4:8]]!=[recipe['details']['authored_health']]*2+[recipe['details']['authored_armor']]*2:
        raise RuntimeError('Exchange changed either authored chassis health/armor')
    for values in (pre,end):
        if values['rf_scene_vehicle_state'][5] or values['rf_scene_vehicle_route_state'][0] or values['rf_scene_vehicle_route_state'][3] or any(values['rf_scene_npc_seats']) or any(values['rf_scene_setup_result']):
            raise RuntimeError('Unexpected owner error, NPC binding or scripted route')
        if f(values['rf_scene_vehicle_damage'][0])!=recipe['details']['authored_health'] or values['rf_scene_vehicle_damage'][3]:
            raise RuntimeError('Handoff changed authored living chassis health')
        if values['rf_scene_enemy_combat'][2] or values['rf_scene_enemy_combat'][7]:raise RuntimeError('Unexpected NPC fire/error')
    return dict(result='PASS',outbound_exchange=outbound,return_exchange=apply,first_handle=first_handle,second_handle=state[6],first_remaining=spent_ammo,
                second_remaining=state[10],swaps=2,ordinary_boards=3,ordinary_exits=2,limitations=recipe['limitations'])


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-switch-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'switch');result=run_guest(folder,'switch',hdd,FRAMES,600,snapshot=True,
            extra_symbols=SYMBOLS,probe=live_probe,probe_frame=190,allow_guest_error=True)
        report['native']=result;report.update(validate(result,recipe))
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
