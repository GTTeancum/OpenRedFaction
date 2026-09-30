"""Enemy-free Xbox turret damage/model check; no images, input or playthrough.

Copies one complete authored L2S2a turret record into CTF06, changing only its
transform. The process-local fixture idles AI and sends ordinary handgun-valued
hitscan rays through shared collision/cover/damage. This does not test weapon
input/ammo/cadence or autonomous turret combat. --prepare-only reads assets only.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import struct
from build_fragment_platform_fixture import read_entry, U, F
from inspect_levels import inspect
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

UID = 5547
FRAMES = 240
SYMBOLS = {'rf_scene_turret_test':22, 'rf_scene_turret_owners':8,
           'rf_scene_turret_resources':6, 'rf_scene_turret_draw':4}


def entity_rows(level):
    """Existing v180 independent entity layout, with complete raw record spans."""
    meta=inspect(io.BytesIO(level),dict(offset=0,size=len(level),name='fixture.rfl'))
    section=next((s for s in meta['sections'] if s['type']=='0x30000'),None)
    if section is None:return []
    data=level[section['offset']+8:section['offset']+8+section['size']];pos=4
    def take(n):
        nonlocal pos
        if n<0 or pos+n>len(data):raise ValueError('Entity outside section')
        value=data[pos:pos+n];pos+=n;return value
    def string():return take(struct.unpack('<H',take(2))[0])
    rows=[]
    for _ in range(struct.unpack_from('<I',data)[0]):
        start=pos;uid=struct.unpack('<I',take(4))[0];name=string();transform=pos-start
        take(48);string();take(13);string();string();take(29)
        for _ in range(7):string()
        take(18);flags=take(17)
        if flags[-1] not in (0,1):raise ValueError('Entity trailing flag')
        if flags[-1]:take(4)
        string();string()
        rows.append(dict(uid=uid,name=name.decode('cp1252'),transform=transform,
                         raw=data[start:pos],offset=section['offset']+8+start))
    if pos!=len(data):raise ValueError('Unconsumed entity bytes')
    return rows


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L2S2a.rfl')
    owner=next(r for r in entity_rows(source) if r['uid']==UID)
    if owner['name']!='Stationary Turret':raise ValueError('Unexpected authored class')
    original=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    if entity_rows(original):raise ValueError('CTF06 is no longer enemy-free')
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    start=next(s for s in meta['sections'] if s['type']=='0x70000')
    spawn=struct.unpack_from('<3f',original,start['offset']+8)
    # Head eye is y=-.530805886 in the live authored model. Place its eye
    # at original spawn eye height, four metres down the clear viewing axis.
    position=(spawn[0],spawn[1]+.530805886,spawn[2]-4)
    raw=bytearray(owner['raw']);at=owner['transform']
    raw[at:at+48]=F(*position,0,0,1,1,0,0,0,1,0) # disk forward/right/up
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={};added=0
    for section in meta['sections']:
        kind=int(section['type'],16);payload=original[section['offset']+8:section['offset']+8+section['size']]
        if kind==0 and 0x30000 not in offsets:
            entity=U(1)+raw;offsets[0x30000]=len(out);out+=U(0x30000,len(entity))+entity;added=1
        if kind==0x30000:payload=U(1)+raw
        if kind in (0x600,0x60000):payload=U(0) # no authored scripts/triggers
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    check=entity_rows(out)
    if len(check)!=1 or check[0]['uid']!=UID:raise ValueError('Fixture entity mismatch')
    size=4096+((len(out)+2047)&~2047);archive=bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057]=b'ctf06.rfl';struct.pack_into('<I',archive,2108,len(out));archive[4096:4096+len(out)]=out
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp';path.write_bytes(archive)
    recipe=dict(source_archive='levels1.vpp',source_level='L2S2a.rfl',uid=UID,
        class_name=owner['name'],source_offset=owner['offset'],record_bytes=len(raw),
        source_record_sha256=hashlib.sha256(owner['raw']).hexdigest(),spawn=spawn,position=position,
        staged_fields=['transform','runtime catatonic action'],entities=1,
        scope='Authored class/vitals/models; isolated shared hitscan and death-model selection only')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path


def f(word):return struct.unpack('<f',struct.pack('<I',word))[0]


def validate(result):
    x=result['extra'];t=x['rf_scene_turret_test'];owners=x['rf_scene_turret_owners']
    resources=x['rf_scene_turret_resources'];draw=x['rf_scene_turret_draw']
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024:
        raise RuntimeError('Missing complete bounded stock64MiB run')
    if t[0:2]!=[UID,1] or not t[2] or t[2]!=t[3] or t[3]!=t[5] or t[4] or t[6]!=1 or t[20] or t[21]!=1:
        raise RuntimeError(f'Turret hits/death failed: {t}')
    if f(t[7])<=0 or f(t[8])>0 or f(t[14])>=f(t[7]) or f(t[15])<=0 or f(t[16])!=.5 or f(t[17])<=0:
        raise RuntimeError(f'Turret health/factor mismatch: {t}')
    if t[11]==t[12] or t[13]!=t[12] or resources[0]!=2 or resources[1]!=1:
        raise RuntimeError(f'Live/dead resource selection failed: {t}, {resources}')
    if owners[0]!=1 or owners[2]!=t[5] or owners[3]!=1 or owners[7]:
        raise RuntimeError(f'Unexpected owner damage accounting: {owners}')
    if not draw[0] or not draw[1] or draw[2] or draw[3]!=UID:
        raise RuntimeError(f'Live/dead model submission absent: {draw}')
    if result['free_pages']<=0:raise RuntimeError('No free memory')
    return dict(result='PASS',hits=t[5],initial_health=f(t[7]),final_health=f(t[8]),
        first_hit_health=f(t[14]),handgun_damage=f(t[17]),damage_factor=f(t[16]),
        free_pages=result['free_pages'],live_submissions=draw[0],dead_submissions=draw[1],
        limitations='Draw submission verified; appearance/audio not inspected. No weapon-input/ammo/fire-rate, AI or save claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare generated archive only; no build/emulator')
    args=parser.parse_args()
    if args.prepare_only:
        print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('turret-combat-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-turret-test.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':__doc__}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-turret-test.bin').write_bytes(U(UID))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'combat')
        result=run_guest(folder,'combat',hdd,FRAMES,420,snapshot=True,extra_symbols=SYMBOLS)
        report['native']=result;report.update(validate(result))
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
