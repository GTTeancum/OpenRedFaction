"""Bounded authored Fighter8955 -> Jeep7629 ordinary Use, fire and save.

Original entity bytes and CTF06 geometry are preserved except transforms,
spawn and empty setup sections. Native execution is parent-owned; no route,
host input, images, fabricated owners or direct gameplay state writes.
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
from inspect_models import inspect as inspect_model
from xemu_turret_combat import entity_rows,f
from xemu_vehicle_mixed_types import metadata,model_geometry,diagnostics,SYMBOLS as BASE_SYMBOLS,IDENTITY
from xemu_vehicle_switch_save import check_run
from xemu_npc_jeep_detached_save import component
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

FIGHTER,JEEP=8955,7629
FRAMES=380
LOAD_FRAMES=240
SYMBOLS=dict(BASE_SYMBOLS,rf_scene_fighter_weapon=8,rf_scene_vehicle_profile_packs=16,
    rf_scene_checkpoint_world_reject=9,rf_scene_vehicle_player_capture=8,
    rf_scene_vehicle_switch_restore=16,rf_scene_world_load_reject=3)


def replay():
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,
        int(frame in (90,210,270)),int(130<=frame<150 or 310<=frame<320),0,
        int(frame==290),0) for frame in range(FRAMES))


def load_replay():
    # Jeep+X exit meets the room wall; -X exit is about x=.41. The saved
    # identity look makes negative side world-X. Twenty frames at speed6 /
    # acceleration20 cross the x0 owner bisector while remaining in the gap.
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',-float(80<=frame<100),0,0,0,0,0,0,
        int(frame in (60,130)),int(160<=frame<180),0,0,0) for frame in range(LOAD_FRAMES))


def fighter_model():
    data=read_entry(ROOT/'Installed_Game/meshes.vpp','Fighter01.v3m')
    spheres=[];tags=[]
    for section in inspect_model(data)['sections']:
        if section['type']=='0x43535048':
            at=section['offset']+8;parent,x,y,z,radius=struct.unpack_from('<i4f',data,at+24)
            if parent!=-1 or radius<=0:raise RuntimeError('Unexpected original Fighter sphere')
            spheres.append(dict(center=[x,y,z],radius=radius))
        if section.get('lods'):
            lod=section['lods'][0]
            for i in range(lod['props']):
                at=lod['attachment_offset']+100*i;name=data[at:at+68].split(b'\0')[0].decode()
                if name.startswith('interface_'):tags.append(dict(name=name,position=list(struct.unpack_from('<3f',data,at+84))))
    if len(spheres)!=2 or len(tags)!=1:raise RuntimeError('Original Fighter collision/seat metadata changed')
    return dict(model='Fighter01.v3m',sha256=hashlib.sha256(data).hexdigest(),spheres=spheres,tags=tags)


def prepare_level(folder):
    folder=Path(folder);original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('Expected empty original CTF06 entity section')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    owners=[];records=[]
    specs=[('levels3.vpp','L13S3.rfl',FIGHTER,'Fighter01',5,[-3.,1.13,0.],fighter_model()),
           ('levels2.vpp','L12S1.rfl',JEEP,'Jeep01',3,[3.,-.530839687,0.],model_geometry('Jeep01.v3m'))]
    for arc,level,uid,cls,profile,pos,model in specs:
        row=next(r for r in entity_rows(read_entry(ROOT/'Installed_Game'/arc,level)) if r['uid']==uid)
        info=metadata(row)
        if row['name']!=cls or info['seat_host_uid']!=-1 or info['creation_flags']:raise RuntimeError('Expected visible unattached original vehicle')
        raw=bytearray(row['raw']);at=row['transform'];raw[at:at+48]=F(*pos,*IDENTITY)
        if raw[:at]!=row['raw'][:at] or raw[at+48:]!=row['raw'][at+48:]:raise RuntimeError('Non-transform metadata changed')
        spheres=[dict(center=[pos[k]+sphere['center'][k] for k in range(3)],radius=sphere['radius']) for sphere in model['spheres']]
        bounds=[[min(v['center'][k]-v['radius'] for v in spheres),max(v['center'][k]+v['radius'] for v in spheres)] for k in range(3)]
        if bounds[0][0]<-4.75 or bounds[0][1]>5.75 or bounds[1][0]<-1.25 or bounds[1][1]>=3 or bounds[2][0]<-5 or bounds[2][1]>5:raise RuntimeError('Original spheres exceed known central room envelope')
        records.append(bytes(raw));owners.append(dict(uid=uid,profile=profile,metadata=info,position=pos,
            class_name=cls,source_archive=arc,source_level=level,source_sha256=hashlib.sha256(row['raw']).hexdigest(),
            model=model,bounds=bounds,world_spheres=spheres))
    gap=min(math.dist(a['center'],b['center'])-a['radius']-b['radius'] for a in owners[0]['world_spheres'] for b in owners[1]['world_spheres'])
    if gap<2:raise RuntimeError('Insufficient inter-hull exit gap')
    spawn=(-3.,1.4513111,-4.)
    replacements={0x30000:U(2)+b''.join(records),0x600:U(0),0x60000:U(0),0x40000:U(0),0x70000:F(*spawn,*IDENTITY)}
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
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L13S3.rfl'))
    if [r['raw'] for r in entity_rows(out)]!=records:raise RuntimeError('Entity metadata roundtrip changed')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('L13S3.rfl',out)])
    recipe=dict(status='PREPARED_NOT_RUN',scope=__doc__,owners=owners,spawn=spawn,frames=FRAMES,
        geometry=dict(source='levelsm.vpp/ctf06.rfl',floor_y=-1.25,ceiling_y=3,unchanged=True,hull_gap=gap,
            fighter_hover_reason='Ground minimum0.577 puts low interface below the standing floor; y1.13 admits cockpit body while keeping hull below ceiling.'),
        selection=dict(uid=FIGHTER,profile=5,legacy_level='L13S3.rfl'),
        replay=dict(board_Fighter=90,fire_Fighter=[130,149],probe_Fighter=190,exit_Fighter=210,
            board_Jeep=270,Jeep_gunner=290,fire_Jeep=[310,319],save=FRAMES),
        limitations='Numerically prepared hover/exit geometry requires native admission. No rocket expenditure, fresh-load continuation, route, flight maneuver or audiovisual claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');(folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def live_probe(monitor,mapping):
    value={name:words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    value['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return value


def saved_fighter(payload):
    data=component(payload,11)
    if data[:4]!=b'RFSW':raise RuntimeError('Missing RFSW switched-owner envelope')
    version,size,uid,profile,count=struct.unpack_from('<5I',data,4)
    if (version,uid,profile,count)!=(4,JEEP,3,1) or len(data)!=24+size+320:raise RuntimeError('Expected RFSW4 active Jeep and one parked Fighter')
    bank=list(struct.unpack('<80I',data[24+size:]));inner=data[24:24+size]
    if inner[:4]!=b'RFVA' or struct.unpack_from('<3I',inner,4)!=(2,160,1) or len(inner)!=256:raise RuntimeError('Expected route-free occupied Jeep and passive Fighter')
    active=inner[16:176];passive=list(struct.unpack('<20I',inner[176:]))
    if active[:4]!=b'RFVC' or struct.unpack_from('<2I',active,4)!=(3,160) or struct.unpack_from('<2I',active,16)!=(3,3) or struct.unpack_from('<I',active,108)[0]!=1:raise RuntimeError('Jeep occupied gunner state missing')
    if bank[:2]!=[FIGHTER,1] or bank[15]!=5 or passive[:2]!=[FIGHTER,0] or passive[19] or bank[16:28]!=passive[2:14]:raise RuntimeError('Parked Fighter identity, alive state or pose changed')
    return dict(primary=bank[4],secondary=bank[5],primary_clock=bank[40:44],secondary_clock=bank[44:48],
        health=passive[14],armor=passive[15],position=passive[2:5],Jeep_ammo=struct.unpack_from('<I',active,104)[0],
        Jeep_rng=struct.unpack_from('<I',active,120)[0],Jeep_health=struct.unpack_from('<I',active,24)[0],
        Jeep_armor=struct.unpack_from('<I',active,28)[0])


def validate(guest,payload):
    check_run(guest,FRAMES);pre,end=guest['probe'],guest['extra']
    if guest['player_life'][2] or end['rf_scene_combat'][0]:raise RuntimeError('Unexpected player death or handheld fire')
    if not 190<=pre['frame']<210 or pre['rf_scene_vehicle_enabled']!=[5] or pre['rf_scene_vehicle_state'][1:4]!=[1,0,1] or pre['rf_scene_vehicle_switch'][1]:raise RuntimeError('Original Fighter ordinary boarding not established')
    weapon=pre['rf_scene_fighter_weapon']
    if weapon[1]<1 or weapon[5] or weapon[6]!=900-weapon[1] or weapon[2] or weapon[7]!=20:raise RuntimeError('Finite actual Fighter minigun fire or quiet endpoint not established')
    sw=end['rf_scene_vehicle_switch'];a=end['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:6]!=[FIGHTER,JEEP] or sw[8] or sw[15]!=JEEP or a[:4]!=[FIGHTER,JEEP,pre['rf_scene_vehicle_state'][12],end['rf_scene_vehicle_state'][12]] or a[8]!=weapon[6] or a[10]!=20 or a[29]!=1 or not 269<=a[30]<=271:raise RuntimeError(f'Ordinary Fighter-to-Jeep ownership exchange failed: {sw}')
    if end['rf_scene_vehicle_enabled']!=[3] or end['rf_scene_vehicle_state'][1:4]!=[2,1,1] or end['rf_scene_jeep_seats'][4]!=1 or end['rf_scene_apc_primary'][1]<1 or end['rf_scene_apc_primary'][7]>=a[9]:raise RuntimeError('Jeep did not board/fire its actual weapon')
    if [f(v) for v in a[4:8]]!=[50.,400.,0.,0.]:raise RuntimeError('Distinct original vehicle vitals were not retained')
    state=guest['checkpoint_state']
    if state[9]!=1 or state[3] or state[4]!=len(payload):raise RuntimeError('Ordinary occupied Jeep save failed')
    saved=saved_fighter(payload)
    clock=saved['primary_clock'];secondary=saved['secondary_clock']
    if not math.isfinite(f(clock[0])) or f(clock[0])>0 or not math.isfinite(f(clock[1])) or f(clock[1])>0 or clock[2] or clock[3]!=weapon[1] or not math.isfinite(f(secondary[0])) or f(secondary[0])>0 or any(secondary[1:]):raise RuntimeError('Parked Fighter shot clock or unused rocket state changed')
    if saved['primary']!=weapon[6] or saved['secondary']!=20 or saved['position']!=a[12:15] or saved['health']!=a[4] or saved['armor']!=a[6] or saved['Jeep_ammo']!=end['rf_scene_apc_primary'][7]:raise RuntimeError('Saved owner ammunition, pose or vitals changed')
    return dict(result='PASS',saved=saved,exchange=a,free_pages=guest['free_pages'],limitations='Source switch/save only; fresh-load and rocket expenditure unverified.')


def validate_fresh_load(source,loaded,payload):
    saved=validate(source,payload)['saved'];check_run(loaded,LOAD_FRAMES)
    pre,end=loaded['probe'],loaded['extra'];state=loaded['checkpoint_state']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(end['rf_scene_world_load_reject']):raise RuntimeError('Ordinary RFSW4 fresh load failed')
    if loaded['player_life'][2] or end['rf_scene_combat'][0]:raise RuntimeError('Unexpected player death or handheld fire')
    if not 40<=pre['frame']<60 or pre['rf_scene_vehicle_enabled']!=[3] or pre['rf_scene_vehicle_state'][1:4]!=[0,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][7]!=saved['Jeep_ammo']:raise RuntimeError('Saved occupied Jeep did not restore without boarding')
    restore=end['rf_scene_vehicle_switch_restore']
    expected=[1,JEEP,3,1,1,FIGHTER,saved['primary'],0,0,0,0,saved['Jeep_ammo'],saved['Jeep_rng'],pre['rf_scene_vehicle_state'][12]]
    if restore[:14]!=expected or len(restore)!=16 or restore[14] in (0,0xffffffff,restore[13]) or restore[15] or pre['rf_scene_vehicle_switch_restore']!=restore:raise RuntimeError('Immutable Fighter/Jeep bank, RNG or owner restore changed')
    if pre['rf_scene_vehicle_damage'][0]!=saved['Jeep_health'] or pre['rf_scene_vehicle_switch'][1] or pre['rf_scene_apc_primary'][1] or pre['rf_scene_fighter_weapon'][1] or pre['rf_scene_fighter_weapon'][2]:raise RuntimeError('Fresh load changed active health or replayed switch/fire')
    return dict(result='PASS',scope='Ordinary occupied Jeep fresh-load before return inputs',saved=saved,restore=restore,
        limitation='Parked Fighter secondary supply/vitals and passive handle are independently checked only by actual return.')


def validate_return(loaded,fresh):
    saved=fresh['saved'];restore=fresh['restore'];end=loaded['extra'];sw=end['rf_scene_vehicle_switch'];a=end['rf_scene_vehicle_switch_apply']
    if sw[1]!=1 or sw[4:8]!=[JEEP,FIGHTER,restore[13],restore[14]] or sw[8] or sw[9]!=1 or sw[15]!=FIGHTER:raise RuntimeError(f'Ordinary return to parked Fighter failed: {sw}')
    if end['rf_scene_vehicle_enabled']!=[5] or end['rf_scene_vehicle_state'][1:4]!=[1,1,1] or end['rf_scene_vehicle_state'][12]!=restore[14]:raise RuntimeError('Fighter ordinary boarding or restored handle missing')
    if a[:4]!=[JEEP,FIGHTER,restore[13],restore[14]] or a[8:12]!=[saved['Jeep_ammo'],saved['primary'],0,saved['secondary']] or a[15:18]!=saved['position'] or a[24:30]!=[1,0,0,0,0,1] or a[31] or not 129<=a[30]<=131:raise RuntimeError('Return changed saved ownership, ammunition, pose or input timing')
    if a[4:8]!=[saved['Jeep_health'],saved['health'],saved['Jeep_armor'],saved['armor']] or end['rf_scene_vehicle_damage'][0]!=saved['health']:raise RuntimeError('Return changed original health50/400 or armor')
    weapon=end['rf_scene_fighter_weapon']
    if weapon[1]<1 or weapon[2] or weapon[5] or weapon[6]!=saved['primary']-weapon[1] or weapon[7]!=saved['secondary'] or end['rf_scene_apc_primary'][1]:raise RuntimeError('Returned Fighter did not spend its own restored minigun supply without phantom launches')
    return dict(result='PASS',return_exchange=a,return_weapon=weapon,free_pages=loaded['free_pages'],
        limitations='One ordinary Fighter/Jeep source-save and fresh-load return; rocket expenditure, moving transfer and audiovisual fidelity unverified.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--resume-saved-run',type=Path,help='Validate retained source and run only fresh-load return using existing save HDD')
    args=parser.parse_args()
    if args.prepare_only:
        path,recipe=prepare_level(args.prepare_only);print(path);return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('vehicle-fighter-switch-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L13S3.rfl'.ljust(64,b'\0'))
        for flag in ('campaign-spawn.flag','player-control.flag'):(DISC/flag).write_bytes(b'')
        if args.resume_saved_run:
            prior=args.resume_saved_run.resolve();source=json.loads((prior/'save/result.json').read_text())
            payload=(prior/'save/xbox-world.rfwc').read_bytes()
            report.update(resumed_source=str(prior),source_validation=validate(source,payload))
            report['load_schedule']=dict(frames=LOAD_FRAMES,probe=40,exit_Jeep=60,left=[80,99],board_Fighter=130,fire_Fighter=[160,179])
            (DISC/'world-hdd-load.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(load_replay())
            build(folder,'load');loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,360,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=40,allow_guest_error=True)
            report.update(load=loaded,diagnostics_load=diagnostics(loaded))
            report['fresh_load']=validate_fresh_load(source,loaded,payload)
            report.update(validate_return(loaded,report['fresh_load']))
        else:
            (DISC/'world-hdd-save.flag').write_bytes(b'1');(DISC/'player-replay.bin').write_bytes(replay())
            build(folder,'save');guest=run_guest(folder,'save',hdd,FRAMES,420,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=190,allow_guest_error=True)
            report.update(guest=guest,diagnostics=diagnostics(guest))
            payload=(folder/'save/xbox-world.rfwc').read_bytes();report.update(validate(guest,payload))
    except Exception as exc:
        report['error']=str(exc)
        if report.get('fresh_load',{}).get('result')=='PASS':report['partial_result']='FRESH_LOAD_PASS_RETURN_FAILED'
        raise
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
