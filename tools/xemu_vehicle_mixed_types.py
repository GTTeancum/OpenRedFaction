"""Bounded original Jeep/APC Xbox handoff fixture; runtime integration pending.

Complete original records retain their true UIDs and non-transform metadata.
The optional runtime check is gated until the parent enables the real resource
transaction. Process-local ordinary Use/seat/fire only; no images/host input.
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
from xemu_turret_combat import entity_rows
from xemu_npc_turret_seat import record_details
from xemu_vehicle_switch import SYMBOLS as SWITCH_SYMBOLS
from xemu_turret_combat import f
from xemu_native_world_save import DISC,FLAGS,build,run_guest,address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ROOT=Path(__file__).resolve().parents[1]
JEEP,APC=7629,3303
FLOOR=-1.25
IDENTITY=(0,0,1,1,0,0,0,1,0)
MIRRORED=(0,0,-1,-1,0,0,0,1,0)
FRAMES=380
# Parent flips this only after transactional profile3 -> profile2 ownership,
# resource and physics handoff is enabled. Preparation remains usable now.
CROSS_CLASS_RUNTIME_READY=False
SYMBOLS=dict(SWITCH_SYMBOLS,rf_scene_vehicle_enabled=1,rf_scene_combat=8)


def replay():
    # Jeep faces+Z; ordinary saved upright exit basis retains right=+X.
    # A short approach crosses the nearest-owner bisector without forced pose.
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',float(230<=frame<246),0,0,0,0,0,0,
        int(frame in (90,210,270)),int(130<=frame<150 or 300<=frame<320),0,
        int(frame==110),0) for frame in range(FRAMES))


def metadata(row):
    """v180 creation switches, same offsets as rf_level_entity_spawn_read."""
    raw=row['raw'];at=row['transform']+48
    def string():
        nonlocal at
        length=struct.unpack_from('<H',raw,at)[0];at+=2+length
        if at>len(raw):raise RuntimeError('Invalid original entity string')
    string();at+=13;string();string();at+=29
    for _ in range(7):string()
    at+=18
    return dict(record_details(row),creation_flags=(2 if raw[at+1] else 0)|(4 if raw[at+15] else 0))


def model_geometry(name):
    data=read_entry(ROOT/'Installed_Game/meshes.vpp',name);parsed=inspect_model(data)
    spheres=[];tags=[]
    for section in parsed['sections']:
        if section['type']=='0x43535048':
            if section['declared']!=44:raise RuntimeError('Unexpected CSPH record size')
            at=section['offset']+8;parent,x,y,z,radius=struct.unpack_from('<i4f',data,at+24)
            if parent!=-1 or radius<=0 or not all(math.isfinite(v) for v in (x,y,z,radius)):
                raise RuntimeError('Expected finite unparented vehicle collision sphere')
            spheres.append(dict(name=data[at:at+24].split(b'\0')[0].decode(),center=[x,y,z],radius=radius))
        if section.get('lods'):
            lod=section['lods'][0]
            for i in range(lod['props']):
                at=lod['attachment_offset']+100*i;tag=data[at:at+68].split(b'\0')[0].decode()
                if tag.startswith('interface_'):
                    tags.append(dict(name=tag,index=i,position=list(struct.unpack_from('<3f',data,at+84)),parent=struct.unpack_from('<i',data,at+96)[0]))
    if len(spheres)!=6 or not tags:raise RuntimeError('Original vehicle sphere/seat metadata changed')
    return dict(model=name,sha256=hashlib.sha256(data).hexdigest(),spheres=spheres,tags=tags)


def place(model,x,mirrored=False):
    # Neither installed class overrides model sphere radii. Four authored
    # spring overrides affect later settling; the initial hull is above floor.
    y=FLOOR-min(s['center'][1]-s['radius'] for s in model['spheres'])+.01
    position=[x,y,0];sign=-1 if mirrored else 1
    spheres=[dict(center=[x+sign*s['center'][0],y+s['center'][1],sign*s['center'][2]],radius=s['radius']) for s in model['spheres']]
    bounds=[[min(s['center'][i]-s['radius'] for s in spheres),max(s['center'][i]+s['radius'] for s in spheres)] for i in range(3)]
    if bounds[0][0]<-4.75 or bounds[0][1]>5.75 or bounds[2][0]<-5 or bounds[2][1]>5 or bounds[1][0]<FLOOR or bounds[1][1]>=3:
        raise RuntimeError('Vehicle collision bounds exceed the central room floor/ceiling envelope')
    return position,spheres,bounds


def prepare_level(folder,preload_only=False):
    folder=Path(folder)
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise RuntimeError('Expected actor-free CTF06')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    sources=[('levels2.vpp','L12S1.rfl',JEEP,'Jeep01','Jeep01.v3m',3,-3,False),
             ('levels3.vpp','L15S1.rfl',APC,'APC','APC.v3m',2,3,True)]
    records=[];owners=[];world_spheres=[]
    for archive_name,level,uid,class_name,model_name,profile,x,mirrored in sources:
        source=read_entry(ROOT/'Installed_Game'/archive_name,level)
        owner=next(r for r in entity_rows(source) if r['uid']==uid)
        details=metadata(owner)
        if owner['name']!=class_name or details['seat_host_uid']!=-1 or details['creation_flags']&6:
            raise RuntimeError('Expected original visible, living, unparented vehicle')
        model=model_geometry(model_name);position,spheres,bounds=place(model,x,mirrored)
        raw=bytearray(owner['raw']);at=owner['transform'];basis=MIRRORED if mirrored else IDENTITY
        raw[at:at+48]=F(*position,*basis)
        if raw[:at]!=owner['raw'][:at] or raw[at+48:]!=owner['raw'][at+48:]:raise RuntimeError('Changed non-transform entity bytes')
        records.append(bytes(raw));world_spheres.append(spheres)
        owners.append(dict(uid=uid,class_name=class_name,profile=profile,source_archive=archive_name,
            source_level=level,source_offset=owner['offset'],source_sha256=hashlib.sha256(owner['raw']).hexdigest(),
            metadata=details,position=position,basis=list(basis),collision_bounds=bounds,model=model,
            authored_use_radius=5 if profile==3 else 8))
    gap=min(math.dist(a['center'],b['center'])-a['radius']-b['radius'] for a in world_spheres[0] for b in world_spheres[1])
    if gap<1:raise RuntimeError('Insufficient numerical hull separation for this staged room')
    spawn=(-3,1.4513111,-2)
    replacements={0x30000:U(2)+b''.join(records),0x600:U(0),0x60000:U(0),0x40000:U(0),
                  0x70000:F(*spawn,*IDENTITY)}
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
    if [r['raw'] for r in entity_rows(out)]!=records:raise RuntimeError('Original vehicle record roundtrip failed')
    distances=[math.dist(spawn,owner['position']) for owner in owners]
    if distances[0]>=distances[1] or distances[0]>=owners[0]['authored_use_radius']:raise RuntimeError('Jeep is not the nearest initially reachable metadata candidate')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';archive(path,[('ctf06.rfl',out)])
    recipe=dict(status='PREPARED_NOT_RUN',runtime_status='PENDING_CROSS_CLASS_INTEGRATION',scope=__doc__,owners=owners,spawn=spawn,
        initial_selection=dict(uid=JEEP,profile=3,source=2),spawn_distances=distances,
        geometry=dict(source='levelsm.vpp/ctf06.rfl',floor_face=171,floor_y=FLOOR,
            floor_central_bounds=[[-4.75,5.75],[-5,5]],ceiling_min_y=3,
            lateral_hull_gap=owners[1]['collision_bounds'][0][0]-owners[0]['collision_bounds'][0][1],
            minimum_sphere_pair_gap=gap,original_geometry_unchanged=True),
        staged_fields=['vehicle transforms','player spawn','empty events/triggers/items'],
        frames=FRAMES,replay={'board_Jeep':90,'gunner_Jeep':110,'fire_Jeep':[130,149],
            'probe_Jeep':190,'exit_Jeep':210,'approach_APC_right':[230,245],
            'board_APC':270,'fire_APC':[300,319],'finish_occupied_APC':FRAMES},
        runtime_requirements='Transactional profile3/2 resource/physics/weapon/seat handoff must be enabled before a native replay is meaningful.',
        limitations='One parked Jeep-to-APC handoff only. Initial geometry is numerical preparation; native checks must establish actual boarding/exit and fire. APC-to-Jeep return, secondary fire, saves, other classes and audiovisual fidelity are outside this check.')
    if preload_only:
        recipe.update(runtime_status='PENDING_PROFILE_PRELOAD_CHECK',frames=120,
            replay={'neutral':[0,119],'live_probe':60},
            runtime_requirements='Integrated profile pack demand/load/material merge and rf_scene_vehicle_profile_packs[16].',
            limitations='Neutral resident-resource preload check only; no boarding, switching, firing, visual content or cross-class gameplay claim.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(120*48) if preload_only else replay())
    return path,recipe


def live_probe(monitor,mapping,symbols=SYMBOLS):
    sample={name:words(monitor,address(mapping,name),count) for name,count in symbols.items()}
    sample['frame']=words(monitor,address(mapping,'rf_diagnostic'),58)[37];return sample


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB mixed-vehicle run')
    pre,end=result['probe'],result['extra'];first=pre['rf_scene_vehicle_state'];last=end['rf_scene_vehicle_state']
    if not 190<=pre['frame']<210 or pre['rf_scene_vehicle_selection']!=[2,2,0,0,0,0,0,2,JEEP,3,2,0] or pre['rf_scene_vehicle_enabled']!=[3]:
        raise RuntimeError('Initial metadata-selected original Jeep/profile3 not established')
    if first[1:4]!=[1,0,1] or pre['rf_scene_jeep_seats'][4]!=1 or pre['rf_scene_apc_primary'][1]<1 or pre['rf_scene_vehicle_switch'][1]:
        raise RuntimeError('Original Jeep did not normally board and fire before handoff')
    if end['rf_scene_vehicle_enabled']!=[2] or last[3]!=1 or last[1]-first[1]!=1 or last[2]-first[2]!=1:
        raise RuntimeError('Ordinary Jeep exit/APC boarding did not publish profile2 ownership')
    sw=end['rf_scene_vehicle_switch'];apply=end['rf_scene_vehicle_switch_apply']
    old_handle,new_handle=first[12],last[12]
    if old_handle in (0,0xffffffff) or new_handle in (0,0xffffffff,old_handle):raise RuntimeError('Distinct original owner handles were lost')
    if sw[1]!=1 or sw[4:8]!=[JEEP,APC,old_handle,new_handle] or sw[8] or sw[9] or sw[15]!=APC:
        raise RuntimeError(f'Cross-class ordinary Use transaction failed: {sw}')
    if apply[:4]!=[JEEP,APC,old_handle,new_handle] or apply[8]!=pre['rf_scene_apc_primary'][7] or apply[24] or apply[25:27]!=[0,0] or apply[29]!=1 or not 269<=apply[30]<=271 or apply[31]:
        raise RuntimeError('Handoff lost stable registry identity, parked Jeep ammo or quiet ownership')
    history=end['rf_scene_vehicle_switch_history']
    if history[:32]!=apply or any(history[32:]):raise RuntimeError('Unexpected repeated profile transaction')
    jeep_health=recipe['owners'][0]['metadata']['authored_health'];apc_health=recipe['owners'][1]['metadata']['authored_health']
    if f(apply[4])!=jeep_health or f(apply[5])!=apc_health or f(pre['rf_scene_vehicle_damage'][0])!=jeep_health or f(end['rf_scene_vehicle_damage'][0])!=apc_health:
        raise RuntimeError('Transaction replaced distinct authored vehicle health with another profile default')
    shots=end['rf_scene_apc_primary'][1]-pre['rf_scene_apc_primary'][1]
    spent=apply[9]-end['rf_scene_apc_primary'][7]
    if shots<=0 or spent!=shots or apply[9]<=0:
        raise RuntimeError(f'New active APC did not launch and debit its own real primary rounds: shots={shots}, debit={spent}')
    for sample in (pre,end):
        if sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_damage'][3] or not sample['rf_scene_vehicle_damage'][7] or any(sample['rf_scene_setup_result']) or any(sample['rf_scene_npc_seats']):
            raise RuntimeError('Vehicle error/destruction or unexpected event/NPC ownership')
        if sample['rf_scene_vehicle_route_state'][0] or sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_vehicle_route_state'][7] or sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_combat'][0]:
            raise RuntimeError('Unexpected route, NPC or handheld fire contaminated vehicle-only check')
    return dict(result='PASS',runtime_status='ONE_DIRECTION_FUNCTIONAL_PASS',exchange=apply,
        jeep_handle=old_handle,apc_handle=new_handle,jeep_shots=pre['rf_scene_apc_primary'][1],
        apc_shots=shots,apc_ammo_spent=spent,free_pages=result['free_pages'],limitations=recipe['limitations'])


def validate_preload(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=120 or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB resident-profile preload check')
    pre,end=result['probe'],result['extra']
    if not 60<=pre['frame']<120:raise RuntimeError('Missing live phase2 preload sample')
    packs=pre['rf_scene_vehicle_profile_packs']
    # Masks contain extra/nonactive profiles only, with bit(profile).
    if packs[:3]!=[1<<2]*3 or packs[3]<=0 or packs[6] or packs[7]<=0:
        raise RuntimeError(f'APC profile was not fully loaded/merged with successful memory admission: {packs}')
    if end['rf_scene_vehicle_profile_packs']!=packs:raise RuntimeError('Preload state changed during neutral hold or teardown')
    for sample in (pre,end):
        if sample['rf_scene_vehicle_selection']!=[2,2,0,0,0,0,0,2,JEEP,3,2,0] or sample['rf_scene_vehicle_enabled']!=[3]:
            raise RuntimeError('Preloading APC changed the metadata-selected Jeep owner/profile')
        if sample['rf_scene_vehicle_state'][1:4]!=[0,0,0] or sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_switch'][1] or sample['rf_scene_apc_primary'][1] or sample['rf_scene_combat'][0]:
            raise RuntimeError('Neutral preload unexpectedly boarded, switched, fired or failed')
        if any(sample['rf_scene_setup_result']) or any(sample['rf_scene_npc_seats']) or sample['rf_scene_vehicle_route_state'][0] or sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7]:
            raise RuntimeError('Unexpected script, route or NPC work in preload-only check')
    if pre['rf_scene_vehicle_state'][12] in (0,0xffffffff) or end['rf_scene_vehicle_state'][12]!=pre['rf_scene_vehicle_state'][12]:
        raise RuntimeError('Active Jeep handle changed during neutral resource preload')
    return dict(result='PASS',runtime_status='PRELOAD_ONLY_PASS_GAMEPLAY_PENDING',
        packs=packs,active_uid=JEEP,active_profile=3,active_handle=pre['rf_scene_vehicle_state'][12],
        resident_bytes=packs[3],free_pages=result['free_pages'],limitations=recipe['limitations'])


def run(preload_only=False):
    if not preload_only and not CROSS_CLASS_RUNTIME_READY:
        raise RuntimeError('Runtime check gated: parent must integrate the real cross-class transaction and enable CROSS_CLASS_RUNTIME_READY; no build or emulator started')
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    mode='preload' if preload_only else 'mixed'
    folder=ROOT/'artifacts/xemu'/('vehicle-mixed-'+mode+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe=prepare_level(folder/'level',preload_only)
    symbols=dict(SYMBOLS,rf_scene_vehicle_profile_packs=16) if preload_only else SYMBOLS
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',recipe=recipe)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes((folder/'level/player-replay.bin').read_bytes());build(folder,mode)
        result=run_guest(folder,mode,hdd,120 if preload_only else FRAMES,600,snapshot=True,extra_symbols=symbols,
            probe=lambda monitor,mapping:live_probe(monitor,mapping,symbols),
            probe_frame=60 if preload_only else 190,allow_guest_error=True)
        report['native']=result;report.update((validate_preload if preload_only else validate)(result,recipe))
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


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare-only',type=Path,help='Write fixture and pending replay without building or launching')
    mode.add_argument('--run',action='store_true',help='Parent-only bounded native check; gated until cross-class transaction is integrated')
    mode.add_argument('--preload-only',action='store_true',help='Parent-only120-frame neutral resident resource check; no switching or gameplay claim')
    args=parser.parse_args()
    if args.prepare_only:
        path,recipe=prepare_level(args.prepare_only);print(path);print(recipe['status']);return
    run(args.preload_only)


if __name__=='__main__':main()
