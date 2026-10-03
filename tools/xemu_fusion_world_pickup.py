"""Bounded Xbox ordinary placed Fusion collection, with no scripted grant.

Copies ctf03 shoulder-cannon item253 into enemy-free CTF06, changing position
only. Neutral landing/collection, one normal cycle at100, finish140. No shots,
images, host input, campaign traversal or fabricated ownership. This exercises
SP collection of an installed multiplayer placement, not an SP campaign item.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry,U
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_native_world_save import ROOT,DISC,FLAGS,build,run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

UID,FRAMES=253,140
SYMBOLS={'rf_scene_pickups':8,'rf_scene_pickup_audio':10,
         'rf_scene_pickup_notice':16,'rf_scene_weapon_selection':8,
         'rf_scene_player_ammo':8,'rf_scene_script_grants':8}


def item_rows(raw,meta):
    section=next(s for s in meta['sections'] if s['type']=='0x40000')
    data=raw[section['offset']+8:section['offset']+8+section['size']]
    count,=struct.unpack_from('<I',data);at=4;rows=[]
    for _ in range(count):
        start=at;uid,=struct.unpack_from('<I',data,at);at+=4
        n,=struct.unpack_from('<H',data,at);at+=2
        name=data[at:at+n].decode('cp1252');at+=n;position=at-start;at+=48
        n,=struct.unpack_from('<H',data,at);at+=2+n+1
        quantity,=struct.unpack_from('<i',data,at);at+=12
        rows.append(dict(uid=uid,name=name,position=position,quantity=quantity,raw=data[start:at]))
    if at!=len(data):raise RuntimeError('Malformed installed item section')
    return rows


def prepare_level(folder):
    source=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf03.rfl')
    meta=inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='ctf03.rfl'))
    row=next(r for r in item_rows(source,meta) if r['uid']==UID)
    if row['name']!='shoulder cannon' or row['quantity']!=1:
        raise RuntimeError('Installed Fusion item identity changed')
    raw=read_entry(ROOT/'Installed_Game/levelsm.vpp','ctf06.rfl')
    meta=inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='ctf06.rfl'))
    spawn=next(s for s in meta['sections'] if s['type']=='0x70000')
    xyz=struct.unpack_from('<3f',raw,spawn['offset']+8)
    # CTF06 face171 is the y=-1.25 floor below this spawn. Keep the item
    # half a metre above it, within the unchanged player's ordinary radius.
    position=(xyz[0],-.75,xyz[2]-.5)
    item=bytearray(row['raw']);struct.pack_into('<3f',item,row['position'],*position)
    replacement={0x40000:U(1)+item,0x30000:U(0),0x600:U(0),0x60000:U(0)}
    out=bytearray(raw[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16)
        payload=replacement.get(kind,raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    check=inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['raw'] for r in item_rows(out,check)]!=[bytes(item)]:
        raise RuntimeError('Fixture failed exact item round trip')
    folder.mkdir(parents=True,exist_ok=True);path=folder/'scene-fixture.vpp'
    archive(path,[('ctf06.rfl',out)])
    recipe=dict(scope=__doc__,source='levelsm.vpp/ctf03.rfl',uid=UID,
                source_sha256=hashlib.sha256(row['raw']).hexdigest(),
                changed_item_fields=['position'],position=position,quantity=1,
                inventory_injection=False,script_grants=False)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path


def replay():
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,
        0,0,0,0,0,int(frame==100),0) for frame in range(FRAMES))


def validate(run):
    if run['guest_phase']!=5 or run['frames']!=FRAMES or run['memory_bytes']!=64*1024*1024 or run['free_pages']<=0:
        raise RuntimeError('Incomplete stock64MiB run')
    x=run['extra'];p=x['rf_scene_pickups'];a=x['rf_scene_player_ammo'];s=x['rf_scene_weapon_selection']
    if p[3:6]!=[1,1,UID] or p[7] or any(x['rf_scene_script_grants']):
        raise RuntimeError(f'Expected one ordinary Fusion collection and no scripted grant: {p}')
    if s[:2]!=[12,1] or a[0]==0xffffffff or a[1:3]!=[0,1] or a[7]:
        raise RuntimeError(f'Collected Fusion could not be selected with its one real shell: {s}, {a}')
    audio=x['rf_scene_pickup_audio']
    if audio[:6]!=[1,1,0,0,UID,12] or not audio[6]:
        raise RuntimeError(f'Ordinary accepted weapon-pickup audio was not submitted once: {audio}')
    notice=struct.pack('<16I',*x['rf_scene_pickup_notice']).split(b'\0',1)[0]
    if notice!=b'Fusion Rocket Launcher and 1 shell picked up':
        raise RuntimeError(f'Wrong authored pickup notice: {notice!r}')
    return dict(result='PASS',collected_uid=UID,selected_slot=12,loaded_shells=1,
                limits='No firing, visual/audio-output inspection, MP respawn or campaign placement claim.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',type=Path);args=parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only));return
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('fusion-world-pickup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL')
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(replay());build(folder,'pickup')
        report['guest']=run_guest(folder,'pickup',hdd,FRAMES,360,snapshot=True,extra_symbols=SYMBOLS)
        report.update(validate(report['guest']))
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
