"""Prepare-only original Jeep/APC testbed for future Xbox cross-class handoff.

No build, emulator launch, replay, ownership override or runtime pass claim.
Complete original records retain their true UIDs and non-transform metadata.
"""
import argparse
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

ROOT=Path(__file__).resolve().parents[1]
JEEP,APC=7629,3303
FLOOR=-1.25
IDENTITY=(0,0,1,1,0,0,0,1,0)
MIRRORED=(0,0,-1,-1,0,0,0,1,0)


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


def prepare_level(folder):
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
    recipe=dict(status='PREPARED_NOT_RUN',scope=__doc__,owners=owners,spawn=spawn,
        initial_selection=dict(uid=JEEP,profile=3,source=2),spawn_distances=distances,
        geometry=dict(source='levelsm.vpp/ctf06.rfl',floor_face=171,floor_y=FLOOR,
            floor_central_bounds=[[-4.75,5.75],[-5,5]],ceiling_min_y=3,
            lateral_hull_gap=owners[1]['collision_bounds'][0][0]-owners[0]['collision_bounds'][0][1],
            minimum_sphere_pair_gap=gap,original_geometry_unchanged=True),
        staged_fields=['vehicle transforms','player spawn','empty events/triggers/items'],
        future_flow=['ordinary Jeep board and gunner fire','ordinary exit and approach APC',
                     'ordinary APC board/fire/exit','ordinary approach and return to original Jeep'],
        runtime_requirements='Transactional profile3/2 resource/physics/weapon/seat handoff must be enabled before a native replay is meaningful.',
        limitations='Numerical initial hull bounds only; no runtime boarding, exit sweep, suspension settling, cross-class resource transaction, memory budget or save continuity verified. No replay schedule is asserted.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path,recipe


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,required=True,help='Output fixture directory; this tool never builds or launches')
    args=parser.parse_args();path,recipe=prepare_level(args.prepare_only)
    print(path);print(recipe['status'])


if __name__=='__main__':main()
