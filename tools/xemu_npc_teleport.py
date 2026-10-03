"""Bounded Xbox authored delayed NPC teleport and real Jeep-seat release.

Uses original L12S1 miner7646/Jeep7629 and Teleport9711 in empty CTF06.
Only entity transforms and event destination are staged; original2.6s delay,
event basis, links and NPC vitals/loadout remain. No images, routes or host input.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import pefile

from build_fragment_platform_fixture import read_entry, U, F
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_events import inspect as inspect_events
from inspect_levels import inspect
from xemu_npc_jeep_gunner import prepare_level as prepare_pair
from xemu_npc_jeep_seat import ACTOR, HOST, SYMBOLS as SEAT_SYMBOLS, f
from xemu_npc_jeep_detached_save import STOP
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

TELEPORT = 9711
FRAMES = 250
DESTINATION = (4.0,0.0,-3.0)
SYMBOLS = dict(SEAT_SYMBOLS,rf_scene_npc_jeep_detached_probe=16,
               rf_scene_npc_teleport=8,rf_scene_npc_teleport_apply=40)


def original_teleport():
    source = read_entry(ROOT/'Installed_Game/levels2.vpp','L12S1.rfl')
    meta = inspect(io.BytesIO(source),dict(offset=0,size=len(source),name='L12S1.rfl'))
    section = next(s for s in meta['sections'] if s['type']=='0x600')
    data = source[section['offset']+8:section['offset']+8+section['size']]
    pe = pefile.PE(str(ROOT/'Installed_Game/RF.exe'))
    image = pe.get_memory_mapped_image(); base = pe.OPTIONAL_HEADER.ImageBase
    types = []
    for address_ in range(0x5a1a3c,0x5a1ba4,4):
        at = struct.unpack_from('<I',image,address_-base)[0]-base
        types.append(image[at:image.index(b'\0',at)].decode('cp1252'))
    row = next(r for r in inspect_events(data,types) if r['uid']==TELEPORT)
    if row['type_index'] != 4 or row['links'] != [ACTOR,10076,10178] or abs(row['delay']-2.6)>.00001:
        raise ValueError('Original linked delayed Teleport differs')
    raw = data[row['offset']:row['offset']+row['bytes']]
    return row,raw


def prepare_level(folder):
    path,recipe = prepare_pair(folder)
    raw = read_entry(path,'L12S1.rfl')
    meta = inspect(io.BytesIO(raw),dict(offset=0,size=len(raw),name='L12S1.rfl'))
    authored,original = original_teleport(); tele = bytearray(original)
    at = 4+2+len(authored['type'])
    tele[at:at+12] = F(*DESTINATION)
    if tele[:at] != original[:at] or tele[at+12:] != original[at+12:]:
        raise ValueError('Teleport changed beyond destination')
    events = U(2)+event(STOP,'Set_AI_Mode','park_for_teleport',(HOST,))+tele
    out = bytearray(raw[:meta['sections'][0]['offset']]); offsets = {}
    for section in meta['sections']:
        kind = int(section['type'],16)
        payload = events if kind==0x600 else raw[section['offset']+8:section['offset']+8+section['size']]
        offsets[kind] = len(out); out += U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L12S1.rfl'))
    archive(path,[('L12S1.rfl',out)])
    disk = authored['orientation_disk']; runtime = disk[3:6]+disk[6:9]+disk[:3]
    recipe.update(scope=__doc__,frames=FRAMES,replay='neutral',
                  setup_frames={str(STOP):0,str(TELEPORT):60},
                  staged_fields=['entity transforms','ordinary parking event','Teleport destination'],
                  teleport=dict(uid=TELEPORT,authored=authored,destination=DESTINATION,
                                runtime_basis=runtime,source_sha256=hashlib.sha256(original).hexdigest(),
                                requested_frame=60,expected_apply_frame=216),
                  geometry='At x4,z-3 original CTF06 face171 is floor y=-1.25 and face91 ceiling y3. Destination body origin y0 allows ordinary settling.',
                  limitations='OFF control, item/clutter teleport, unrelated attachment families and visuals unverified.')
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    return path,recipe


def replay():
    return b'RFI6'+U(48)+bytes(FRAMES*48)


def live_probe(monitor,mapping):
    result = {name:words(monitor,address(mapping,name),size) for name,size in SYMBOLS.items()}
    result['frame'] = words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    return result


def validate(result,recipe):
    if result['guest_phase']!=5 or result['frames']!=FRAMES or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live,final = result['probe'],result['extra']
    seat,paired = live['rf_scene_npc_seats'],live['rf_scene_npc_seat_probe']
    if not 180<=live['frame']<215 or seat[:4]!=[1,1,0,0] or seat[5:]!=[0,1,ACTOR,HOST,0]:
        raise RuntimeError(f'Delayed teleport did not retain pre-dispatch seat: {seat}')
    if paired[:2]!=[ACTOR,HOST] or paired[6:8]!=[13,paired[3]] or f(paired[4])!=1:
        raise RuntimeError(f'Pre-teleport authored miner ownership lost: {paired}')
    seat,detached = final['rf_scene_npc_seats'],final['rf_scene_npc_jeep_detached_probe']
    if seat[:4]!=[1,1,0,0] or seat[5:]!=[1,0,ACTOR,HOST,0]:
        raise RuntimeError(f'Teleport did not detach exactly once: {seat}')
    if detached[:4]!=[ACTOR,HOST,paired[2],paired[3]] or detached[4]!=paired[4] or detached[5:10]!=[2,0xffffffff,0xffffffff,0xffffffff,0] or detached[15]!=1:
        raise RuntimeError(f'Teleport left stale seat/action/driver or changed health: {detached}')
    position = [f(v) for v in detached[12:15]]
    if not all(math.isfinite(v) for v in position) or max(abs(position[i]-DESTINATION[i]) for i in (0,2))>.1 or not -1.3<position[1]<.1:
        raise RuntimeError(f'Teleported miner snapped back or left valid destination: {position}')
    a,b = live['rf_scene_vehicle_state'],final['rf_scene_vehicle_state']
    if max(abs(f(a[6+i])-f(b[6+i])) for i in range(3))>.05 or live['rf_scene_vehicle_damage']!=final['rf_scene_vehicle_damage']:
        raise RuntimeError('Teleport moved or damaged the parked Jeep')
    for sample in (live,final):
        if sample['rf_scene_vehicle_state'][3] or sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_route_state'][7] or sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or any(sample['rf_scene_script_slays']):
            raise RuntimeError('Unexpected possession, damage, NPC fire or runtime error')
    if final['rf_scene_setup_result']!=[2,TELEPORT,4,0]:
        raise RuntimeError('Original Teleport was not requested through ordinary event dispatch')
    if any(live['rf_scene_npc_teleport']) or any(live['rf_scene_npc_teleport_apply']):
        raise RuntimeError('Teleport ignored its authored delay')
    calls,applied = final['rf_scene_npc_teleport'],final['rf_scene_npc_teleport_apply']
    if calls != [1,1,1,0,TELEPORT,ACTOR,paired[2],0]:
        raise RuntimeError(f'Expected exactly one successful actor teleport/detachment: {calls}')
    if applied[:4] != [1,TELEPORT,ACTOR,paired[2]] or not 3580<=applied[4]<=3620 or applied[5:9] != [paired[3],0xffffffff,2,0] or applied[9] != paired[4]:
        raise RuntimeError(f'Teleport apply timing/identity/action/health failed: {applied}')
    expected = struct.unpack('<12I',F(*DESTINATION,*recipe['teleport']['runtime_basis']))
    if applied[10:22] != list(expected):
        raise RuntimeError('Immediate position/basis differs from original event transform')
    if applied[28:30] != [paired[2],0xffffffff] or applied[30] != applied[31] or f(applied[30])<=0 or applied[32:35] != applied[35:38] or applied[38]:
        raise RuntimeError('Immediate teleport moved/damaged the host or retained its seat/driver')
    if not all(math.isfinite(f(v)) for v in applied[22:28]):
        raise RuntimeError('Invalid preserved motion after teleport')
    return dict(result='PASS',event_uid=TELEPORT,actor_uid=ACTOR,host_uid=HOST,
                requested_frame=60,applied_ms=applied[4],authored_delay=recipe['teleport']['authored']['delay'],
                immediate_position=[f(v) for v in applied[10:13]],final_position=position,
                exact_authored_basis=True,seat_detachments=seat[5],health=f(detached[4]),
                jeep_health=f(applied[31]),free_pages=result['free_pages'],
                limitations=recipe['limitations'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        path,_ = prepare_level(args.prepare_only); print(path); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('npc-teleport-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,recipe = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL',scope=__doc__,recipe=recipe)
    try:
        for name in names: (DISC/name).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'L12S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b''); (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(STOP,TELEPORT))
        (DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'teleport')
        result = run_guest(folder,'teleport',hdd,FRAMES,480,snapshot=True,
                           extra_symbols=SYMBOLS,probe=live_probe,probe_frame=180,allow_guest_error=True)
        report['native'] = result; report.update(validate(result,recipe))
    except Exception as error:
        report['error'] = str(error); raise
    finally:
        for name,data in original.items():
            if data is None: (DISC/name).unlink(missing_ok=True)
            else: (DISC/name).write_bytes(data)
        try: build(folder,'restore')
        except Exception as error:
            report['result']='FAIL'; report['restore_error']=str(error); raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']: report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']: raise RuntimeError('Disc restoration failed')


if __name__=='__main__': main()
