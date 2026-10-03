"""Stock64MiB autonomous opposed-NPC combat in an empty CTF06 testbed.

Two complete guard941 records face each other. One ordinary Set_Friendliness
event makes the second friendly. No Attack, forced damage, player fire, images,
host input or campaign traversal. Parent owns serial build/emulator use.
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
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows
from xemu_npc_turret_seat import record_details
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTORS = (914200, 914201)
FRIENDLY, FRAMES = 914210, 180
SYMBOLS = {'rf_scene_enemy_opposed': 15, 'rf_scene_npc_opposed_probe': 24,
           'rf_scene_enemy_combat': 8, 'rf_scene_enemy_awareness': 8,
           'rf_scene_enemy_retaliation': 4, 'rf_scene_combat': 8,
           'rf_scene_script_attack': 12, 'rf_scene_script_slays': 6,
           'rf_scene_setup_result': 4}


def prepare_level(folder,friendly_delay=0):
    source = read_entry(ROOT/'Installed_Game/levels1.vpp', 'L3S2.rfl')
    guard = next(r for r in entity_rows(source) if r['uid'] == 941)
    details = record_details(guard)
    if guard['name'] != 'guard1' or details['seat_host_uid'] != -1:
        raise RuntimeError('Expected unseated authored guard941')
    raw = read_entry(ROOT/'Installed_Game/levelsm.vpp', 'ctf06.rfl')
    if entity_rows(raw):raise RuntimeError('Expected actor-free testbed')
    meta = inspect(io.BytesIO(raw), dict(offset=0, size=len(raw), name='ctf06.rfl'))
    start = next(s for s in meta['sections'] if s['type'] == '0x70000')
    spawn = struct.unpack_from('<3f', raw, start['offset']+8)
    copies, positions = [], []
    for i, uid in enumerate(ACTORS):
        row = bytearray(guard['raw']);struct.pack_into('<I', row, 0, uid)
        position = (spawn[0]+(-1.5 if i == 0 else 1.5), spawn[1], spawn[2]-3.5)
        direction = 1 if i == 0 else -1
        at = guard['transform']
        row[at:at+48] = F(*position, direction,0,0, 0,0,-direction, 0,1,0)
        copies.append(row);positions.append(position)
    kind, name = 'Set_Friendliness', 'opposed_ally'
    friendly = bytearray(event(FRIENDLY, kind, name, [ACTORS[1]]))
    struct.pack_into('<f', friendly, 4+2+len(kind)+12+2+len(name)+1, friendly_delay)
    struct.pack_into('<I', friendly, 4+2+len(kind)+12+2+len(name)+1+4+2, 2)
    replacements = {0x30000:U(2)+b''.join(copies), 0x600:U(1)+friendly,
                    0x60000:U(0), 0x40000:U(0)}
    present = {int(s['type'],16) for s in meta['sections']}
    out = bytearray(raw[:meta['sections'][0]['offset']]);offsets = {};added = 0
    for sec in meta['sections']:
        k = int(sec['type'],16)
        if k == 0:
            for missing in (0x30000,0x600):
                if missing not in present:
                    data = replacements[missing];offsets[missing] = len(out)
                    out += U(missing,len(data))+data;added += 1
        data = replacements.get(k, raw[sec['offset']+8:sec['offset']+8+sec['size']])
        offsets[k] = len(out);out += U(k,len(data))+data
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    struct.pack_into('<I',out,20,meta['declared_sections']+added)
    inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='ctf06.rfl'))
    if [r['uid'] for r in entity_rows(out)] != list(ACTORS):raise RuntimeError('Actor identity mismatch')
    folder.mkdir(parents=True,exist_ok=True);path = folder/'scene-fixture.vpp'
    archive(path,[('ctf06.rfl',out)])
    (folder/'recipe.json').write_text(json.dumps(dict(scope=__doc__,source_uid=941,
        source_sha256=hashlib.sha256(guard['raw']).hexdigest(),source_details=details,
        actors=ACTORS,positions=positions,changed_fields=['UID','transform'],
        event=dict(uid=FRIENDLY,type=kind,linked_uid=ACTORS[1],affiliation=2,
                   delay_seconds=friendly_delay)),indent=2)+'\n')
    return path, details


def probe(monitor,mapping):
    return {n:words(monitor,address(mapping,n),c) for n,c in SYMBOLS.items()}


def real(word):return struct.unpack('<f',struct.pack('<I',word))[0]


def validate(result, details):
    if result['guest_phase'] != 5 or result['frames'] != FRAMES or result['memory_bytes'] != 64*1024*1024:
        raise RuntimeError('Incomplete stock64MiB combat run')
    x = result['extra'];p = x['rf_scene_npc_opposed_probe']
    acquisition = x['rf_scene_enemy_opposed']
    if acquisition[11] or not acquisition[12] or not acquisition[13]:
        raise RuntimeError(f'Both opposing affiliations did not acquire independently: {acquisition}')
    rows = [p[:12],p[12:]]
    if {r[0] for r in rows} != set(ACTORS):raise RuntimeError(f'Missing actual opposed pair: {rows}')
    for row,other in ((rows[0],rows[1]),(rows[1],rows[0])):
        total = real(row[4])+real(row[5])
        if not math.isfinite(total) or total >= details['authored_health']+details['authored_armor']:
            raise RuntimeError(f'NPC did not take real damage: {rows}')
        if real(row[4]) > 0:
            if real(other[4]) > 0 and (row[2] != other[1] or row[3] != 2):
                raise RuntimeError(f'Living opposed target was lost: {rows}')
            if real(other[4]) <= 0 and (row[2] or row[3]):
                raise RuntimeError(f'Survivor kept attacking a dead target: {rows}')
    if sum(real(row[4]) <= 0 for row in rows) != 1:
        raise RuntimeError(f'Expected this fixture to finish with one survivor: {rows}')
    if result['probe']['rf_scene_enemy_combat'][2:4] != x['rf_scene_enemy_combat'][2:4]:
        raise RuntimeError('Shots continued after the observed death')
    if x['rf_scene_enemy_combat'][2] < 2 or x['rf_scene_enemy_combat'][3] < 2 or x['rf_scene_enemy_combat'][7]:
        raise RuntimeError('No real bilateral combat or combat error')
    if any(x['rf_scene_script_attack']) or any(x['rf_scene_script_slays']) or x['rf_scene_combat'][0]:
        raise RuntimeError('Authored Attack, forced death or player shot contaminated autonomous check')
    if x['rf_scene_setup_result'] != [1,FRIENDLY,30,0]:raise RuntimeError('Friendliness setup failed')
    if result['player_life'][2] or real(x['rf_scene_enemy_combat'][5]) != 100:
        raise RuntimeError('Player was targeted instead of the opposed NPC')
    return dict(result='PASS',rows=rows,shots=x['rf_scene_enemy_combat'][2],
                hits=x['rf_scene_enemy_combat'][3],free_pages=result['free_pages'],
                limitations='One autonomous opposed pair; no visual/audio claim, broad tactics or reactive-target save continuation.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path)
    parser.add_argument('--validate-existing',type=Path)
    args = parser.parse_args()
    if args.validate_existing:
        folder = args.validate_existing
        result = json.loads((folder/'combat/result.json').read_text())
        details = json.loads((folder/'level/recipe.json').read_text())['source_details']
        report = json.loads((folder/'report.json').read_text())
        passed = validate(result,details)
        if report.get('error'):report['initial_validation_error'] = report.pop('error')
        report.update(passed)
        report['validation_correction'] = 'Accept actual fatal damage with survivor target release and no later shots; setup telemetry stores event type30, not1. Original native run unchanged.'
        if not report.get('disc_restored') or report.get('restore_error'):
            raise RuntimeError('Original restoration did not succeed')
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(passed,indent=2));return
    if args.prepare_only:print(prepare_level(args.prepare_only)[0]);return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('npc-opposed-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture,details = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL',scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(FRIENDLY))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(FRAMES*48))
        build(folder,'combat')
        result = run_guest(folder,'combat',hdd,FRAMES,360,snapshot=True,
                           extra_symbols=SYMBOLS,probe=probe,probe_frame=90)
        report['native'] = result;report.update(validate(result,details))
    except Exception as exc:report['error'] = str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:report['result']='FAIL';report['restore_error']=str(exc);raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__ == '__main__':main()
