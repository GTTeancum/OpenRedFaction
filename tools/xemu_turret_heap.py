"""One bounded stock64MiB Xbox authored HEAP turret mount/fire/contact/exit.

Copies the complete L2S2a UID5762 record into enemy-free CTF06, changing only
its transform. Ordinary process-local Use/look/fire reaches the actual HEAP
flight and explosion backend. No fake weapon, forced damage, images or route.
"""
import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry, U, F
from inspect_levels import inspect
from xemu_turret_combat import entity_rows, f
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

UID = 5762
FRAMES = 140
SYMBOLS = {'rf_scene_turret_player': 12, 'rf_scene_turret_player_probe': 19,
           'rf_scene_turret_combat': 10, 'rf_scene_turret_shots': 8,
           'rf_scene_turret_owners': 8, 'rf_scene_turret_test': 22,
           'rf_scene_combat': 8, 'rf_scene_enemy_spread': 8,
           'rf_scene_turret_heap': 12, 'rf_scene_turret_heap_contact': 12,
           'rf_scene_turret_heap_damage': 8}


def prepare_level(folder):
    source = read_entry(ROOT/'Installed_Game/levels1.vpp', 'L2S2a.rfl')
    owner = next(r for r in entity_rows(source) if r['uid'] == UID)
    if owner['name'] != 'Stationary Turret' or b'\x04\0HEAP' not in owner['raw']:
        raise ValueError('Original HEAP turret record differs')
    original = read_entry(ROOT/'Installed_Game/levelsm.vpp', 'ctf06.rfl')
    if entity_rows(original):
        raise ValueError('Original CTF06 contains actors')
    meta = inspect(io.BytesIO(original), dict(offset=0,size=len(original),name='ctf06.rfl'))
    start = next(s for s in meta['sections'] if s['type'] == '0x70000')
    spawn = struct.unpack_from('<3f', original, start['offset']+8)
    position = (spawn[0], spawn[1]-.75, spawn[2]-2)
    raw = bytearray(owner['raw']); at = owner['transform']
    raw[at:at+48] = F(*position, 0,0,-1, -1,0,0, 0,1,0)
    out = bytearray(original[:meta['sections'][0]['offset']]); offsets = {}; added = 0
    for section in meta['sections']:
        kind = int(section['type'],16)
        payload = original[section['offset']+8:section['offset']+8+section['size']]
        if kind == 0 and 0x30000 not in offsets:
            payload_entity = U(1)+raw
            offsets[0x30000] = len(out)
            out += U(0x30000,len(payload_entity))+payload_entity
            added = 1
        if kind == 0x30000: payload = U(1)+raw
        if kind in (0x600,0x60000): payload = U(0)
        offsets[kind] = len(out); out += U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    copied, = entity_rows(out)
    if copied['uid'] != UID or copied['raw'][:at] != owner['raw'][:at] or copied['raw'][at+48:] != owner['raw'][at+48:]:
        raise ValueError('Authored turret bytes changed beyond transform')
    size = 4096+((len(out)+2047)&~2047); archive = bytearray(size)
    struct.pack_into('<4I',archive,0,0x51890ace,1,1,size)
    archive[2048:2057] = b'ctf06.rfl'
    struct.pack_into('<I',archive,2108,len(out)); archive[4096:4096+len(out)] = out
    folder.mkdir(parents=True,exist_ok=True)
    path = folder/'scene-fixture.vpp'; path.write_bytes(archive)
    recipe = dict(source_archive='levels1.vpp',source_level='L2S2a.rfl',uid=UID,
                  source_record_sha256=hashlib.sha256(owner['raw']).hexdigest(),
                  spawn=spawn,position=position,staged_fields=['transform'],
                  authored_weapon='HEAP',runtime_fixture=False,
                  use_frames=[30,120],fire_frames=[45,95],probe_frame=110,
                  geometry='Original horizontal -Z eye ray meets CTF06 barrier at z=-17.5, about16m forward; deeper wall z=-24.5. HEAP blast radius5. Tiny yaw precedes firing; actual contact distance is checked from native telemetry.',
                  scope=__doc__)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path


def replay():
    data = bytearray(b'RFI6'+U(48))
    for frame in range(FRAMES):
        data += struct.pack('<5f7I',0,0,0,0,.02 if 35 <= frame <= 38 else 0,
                            0,0,int(frame in (30,120)),int(45 <= frame <= 95),0,0,0)
    return data


def live_probe(monitor,mapping):
    return {name: words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}


def validate(result):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or result['memory_bytes'] != 64*1024*1024:
        raise RuntimeError('Incomplete bounded stock64MiB run')
    live,final = result['probe'],result['extra']
    mount,end = live['rf_scene_turret_player'],final['rf_scene_turret_player']
    pose,last = live['rf_scene_turret_player_probe'],final['rf_scene_turret_player_probe']
    heap,contact = final['rf_scene_turret_heap'],final['rf_scene_turret_heap_contact']
    if mount[:4] != [1,0,1,UID] or mount[7] or mount[5] != 2:
        raise RuntimeError(f'Ordinary mount/two authored-cadence shots failed: {mount}')
    if end[:4] != [1,1,0,UID] or end[5] != mount[5] or end[7] or end[11]:
        raise RuntimeError(f'Ordinary exit or shot accounting failed: {end}')
    if pose[:2] != [1,UID] or pose[4:7] != [1,1,mount[9]] or mount[8] == mount[9] or 0xffffffff in mount[8:10]:
        raise RuntimeError(f'Mounted ownership mismatch: {pose}, {mount}')
    if last[:2] != [0,UID] or last[4:7] != [0,0,0xffffffff]:
        raise RuntimeError(f'Exit retained ownership: {last}')
    if heap[:6] != [2,2,0,0,0,2] or heap[6] != mount[8] or heap[7] or heap[10]:
        raise RuntimeError(f'HEAP flights/contact/blasts/source failed: {heap}')
    if contact[0] != 1 or not 45 <= contact[1] < 93 or contact[4:7] != [mount[9],mount[8],mount[8]] or contact[11] != 3:
        raise RuntimeError(f'HEAP contact lost real host/player attribution: {contact}')
    point = [f(word) for word in contact[7:10]]
    body = [f(word) for word in pose[8:11]]
    if not all(math.isfinite(v) for v in point+body) or math.dist(point,body) <= 5:
        raise RuntimeError(f'Impact was not safely beyond authored blast radius: {point}, {body}')
    for sample in (live,final):
        control,shots = sample['rf_scene_turret_combat'],sample['rf_scene_turret_shots']
        health = sample['rf_scene_turret_player_probe']
        if not control[2] or control[3] or control[9] or shots[0] != 2 or any(shots[1:]):
            raise RuntimeError(f'Unexpected autonomous/Vauss hitscan behavior: {control}, {shots}')
        if sample['rf_scene_combat'][0] or any(sample['rf_scene_turret_test']) or any(sample['rf_scene_enemy_spread'][:3]) or sample['rf_scene_enemy_spread'][7]:
            raise RuntimeError('Handheld, artificial damage or Vauss instant-shot path ran')
        owners = sample['rf_scene_turret_owners']
        if owners[0] != 1 or owners[2] or owners[3] or owners[7] or f(health[2]) != 400 or f(health[3]) != 100:
            raise RuntimeError(f'Unexpected owner/self-damage: {owners}, {health}')
    if result['free_pages'] <= 0:
        raise RuntimeError('No free memory')
    return dict(result='PASS',uid=UID,weapon='HEAP',shots=heap[0],contacts=heap[1],blasts=heap[5],
                damage_source=heap[6],player_handle=mount[8],host_handle=mount[9],
                first_contact=point,distance_from_player=math.dist(point,body),
                entries=end[0],exits=end[1],free_pages=result['free_pages'],
                limitations='Mounted HEAP flight, physical impact, shared blast dispatch and attribution only. No victim damage, terrain-cut, AI HEAP, save, visual or audible-output claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        path = prepare_level(args.prepare_only)
        (args.prepare_only/'player-replay.bin').write_bytes(replay()); print(path); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('turret-heap-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    archive = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL',scope=__doc__)
    try:
        for name in names: (DISC/name).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(archive.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'player-replay.bin').write_bytes(replay())
        build(folder,'heap')
        result = run_guest(folder,'heap',hdd,FRAMES,420,snapshot=True,
                           extra_symbols=SYMBOLS,probe=live_probe,probe_frame=110,allow_guest_error=True)
        report['native'] = result; report.update(validate(result))
    except Exception as error:
        report['error'] = str(error); raise
    finally:
        for name,data in original.items():
            if data is None: (DISC/name).unlink(missing_ok=True)
            else: (DISC/name).write_bytes(data)
        try:
            build(folder,'restore')
        except Exception as error:
            report['result'] = 'FAIL'; report['restore_error'] = str(error); raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']: report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']: raise RuntimeError('Disc restoration failed')


if __name__ == '__main__': main()
