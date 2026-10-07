"""Bounded Xbox stored vehicle affiliation across switching and fresh load.

Reuse the CTF06 A7629/B915200 Jeep geometry and ordinary source350/load140
switch-save replay. Only B's authored friendliness changes from 1 to 0.
Four stopped-guest, read-only owner snapshots verify A=1/B=0 across active
and passive ownership. No state injection, host input, images or routes.
This measures stored affiliation, not missile targeting/damage or driver
source attribution. The existing RFSW1/RFVA2/RFVC3 contracts stay unchanged.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry
from check_ai_projectile_ordinary import archive
from xemu_turret_combat import entity_rows, f
from xemu_npc_turret_seat import record_details
from xemu_vehicle_switch import A, B
from xemu_vehicle_switch_save import (
    SAVE_FRAMES, LOAD_FRAMES, SYMBOLS, replay, prepare_level as prepare_switch_save,
    validate_source as validate_switch_source, validate as validate_switch_save,
)
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

LIMITATIONS = ('Stored chassis affiliation only, for two same-class Jeeps and one '
    'ordinary save/fresh boot. No actual homing eligibility, missile damage, '
    'occupied-driver source affiliation, current-session load, NPC seats, '
    'cross-profile handoff or audiovisual claim.')


def friendliness_offset(row):
    """The v180 creation field after instance name and five flag bytes."""
    at = row['transform'] + 48
    if at + 2 > len(row['raw']):
        raise RuntimeError('Missing Jeep instance name')
    at += 2 + struct.unpack_from('<H', row['raw'], at)[0] + 5
    if at + 4 > len(row['raw']):
        raise RuntimeError('Jeep friendliness exceeds entity record')
    return at


def prepare_level(folder):
    path, recipe = prepare_switch_save(folder)
    original = read_entry(path, 'ctf06.rfl')
    rows = entity_rows(original)
    if [row['uid'] for row in rows] != [A, B]:
        raise RuntimeError('Established fixture no longer contains the exact Jeep pair')
    offsets = [friendliness_offset(row) for row in rows]
    if any(struct.unpack_from('<I', row['raw'], at)[0] != 1
           for row, at in zip(rows, offsets)):
        raise RuntimeError('Expected both original Jeep creation teams to be friendly')
    patch_at = rows[1]['offset'] + offsets[1]
    patched = bytearray(original)
    struct.pack_into('<I', patched, patch_at, 0)
    if len(patched) != len(original) or any(
            before != after and not patch_at <= i < patch_at + 4
            for i, (before, after) in enumerate(zip(original, patched))):
        raise RuntimeError('Affiliation fixture changed bytes outside B friendliness')
    checked = entity_rows(patched)
    expected_b = bytearray(rows[1]['raw'])
    struct.pack_into('<I', expected_b, offsets[1], 0)
    if [row['raw'] for row in checked] != [rows[0]['raw'], bytes(expected_b)] or any(
            record_details(row) != recipe['details'] for row in checked):
        raise RuntimeError('Exact owner bytes or authored vitals failed fixture round trip')
    archive(path, [('ctf06.rfl', patched)])
    recipe.update(scope=__doc__, limitations=LIMITATIONS,
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        staged_fields=['established pair UID/transforms/spawn', 'B friendliness: 1 to 0'],
        authored_affiliation={str(A): 1, str(B): 0},
        creation_offsets=dict(A=offsets[0], B=offsets[1]),
        affiliation_probes=dict(source_pre=190, source_final=320,
                                load_pre=40, load_final=110),
        unchanged_geometry=True, unchanged_save_formats=['RFSW1', 'RFVA2', 'RFVC3'])
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    return path, recipe


def live_probe(monitor, mapping):
    """run_guest stops the guest before this callback; every operation is a read."""
    sample = {name: words(monitor, address(mapping, name), count)
              for name, count in SYMBOLS.items()}
    sample['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    passive = words(monitor, address(mapping, 'rf_scene_vehicle_homing_owner_layout'), 4)
    affiliation = words(monitor, address(mapping, 'rf_scene_vehicle_affiliation_layout'), 3)
    stride, handle_at, uid_at, health_at = passive
    passive_team_at, active_team_at, active_handle_at = affiliation
    # Both objects embed scene_driller_damage.state.effects. The two emitted
    # passive member offsets supply the same health-to-affiliation displacement
    # for the active object, without an ABI-size or member-offset guess.
    active_health_at = active_team_at + health_at - passive_team_at
    count = words(monitor, address(mapping, 'campaign_passive_vehicle_count'), 1)[0]
    pointer = words(monitor, address(mapping, 'campaign_passive_vehicles'), 1)[0]
    active_pointer = words(monitor, address(mapping, 'scene_driller_damage_owner'), 1)[0]
    if count != 1 or not pointer or pointer % 4 or not active_pointer or active_pointer % 4 or \
       not 0 < stride <= 16384 or any(at % 4 or not 0 <= at <= stride - 4
                                    for at in passive[1:] + [passive_team_at]) or \
       any(at % 4 or not 0 <= at <= 16380
           for at in (active_team_at, active_handle_at, active_health_at)):
        raise RuntimeError('Invalid exact-owner read-only affiliation ABI')
    sample['owner_layout'] = dict(passive=passive, affiliation=affiliation,
                                active_health_offset=active_health_at)
    sample['owner_pointers'] = dict(active=active_pointer, passive=pointer)
    sample['active_owner'] = dict(
        uid=words(monitor, address(mapping, 'campaign_authored_vehicle_uid'), 1)[0],
        handle=words(monitor, active_pointer + active_handle_at, 1)[0],
        affiliation=words(monitor, active_pointer + active_team_at, 1)[0],
        health_bits=words(monitor, active_pointer + active_health_at, 1)[0])
    sample['passive_owners'] = [dict(
        uid=words(monitor, pointer + uid_at, 1)[0],
        handle=words(monitor, pointer + handle_at, 1)[0],
        affiliation=words(monitor, pointer + passive_team_at, 1)[0],
        health_bits=words(monitor, pointer + health_at, 1)[0])]
    return sample


def full_handle(value):
    if not isinstance(value, int) or not 0 < value < 0xffffffff or not value >> 16:
        raise RuntimeError('Missing full generation-bearing vehicle handle')
    return value


def validate_owner_sample(sample, active_uid, frame_min, frame_max, recipe):
    if not frame_min <= sample['frame'] < frame_max:
        raise RuntimeError(f'Missed affiliation sample window [{frame_min}, {frame_max})')
    active = sample['active_owner']; passive = sample['passive_owners']
    if active['uid'] != active_uid or len(passive) != 1 or \
       passive[0]['uid'] != (B if active_uid == A else A):
        raise RuntimeError('Live active/passive owner UID set or role is wrong')
    owners = {owner['uid']: owner for owner in [active] + passive}
    if set(owners) != {A, B}:
        raise RuntimeError('Live affiliation sample duplicated or lost an authored UID')
    handles = {uid: full_handle(owner['handle']) for uid, owner in owners.items()}
    if handles[A] == handles[B] or active['handle'] != sample['rf_scene_vehicle_state'][12]:
        raise RuntimeError('Live damage owner handle differs from active host or aliases parked owner')
    for uid, expected in ((A, 1), (B, 0)):
        if owners[uid]['affiliation'] != expected:
            raise RuntimeError(f'UID{uid} stored affiliation changed: expected {expected}, '
                               f'got {owners[uid]["affiliation"]}')
        if f(owners[uid]['health_bits']) != recipe['details']['authored_health']:
            raise RuntimeError(f'UID{uid} live chassis health differs from authored health')
    if active['health_bits'] != sample['rf_scene_vehicle_damage'][0] or \
       sample['rf_scene_vehicle_damage'][3] or sample['rf_scene_vehicle_state'][5] or \
       sample['rf_scene_vehicle_switch'][8] or any(sample['rf_scene_npc_seats']) or \
       any(sample['rf_scene_setup_result']) or sample['rf_scene_vehicle_route_state'][0] or \
       sample['rf_scene_vehicle_route_state'][3] or sample['rf_scene_enemy_combat'][2] or \
       sample['rf_scene_enemy_combat'][7]:
        raise RuntimeError('Live affiliation sample has owner, health, setup, route or NPC errors')
    return handles


def validate_exchange(before, after, first_uid, frames, recipe):
    handles = validate_owner_sample(before, first_uid, *frames[0], recipe)
    last_uid = B if first_uid == A else A
    final_handles = validate_owner_sample(after, last_uid, *frames[1], recipe)
    if final_handles != handles or before['owner_layout'] != after['owner_layout']:
        raise RuntimeError('Ownership exchange changed full UID/handle identity or telemetry ABI')
    exchange = after['rf_scene_vehicle_switch_apply']
    if before['rf_scene_vehicle_switch'][1] or after['rf_scene_vehicle_switch'][1] != 1 or \
       exchange[:4] != [first_uid, last_uid, handles[first_uid], handles[last_uid]]:
        raise RuntimeError('Live affiliation owners disagree with the single ordinary Use exchange')
    if [f(value) for value in exchange[4:8]] != \
       [recipe['details']['authored_health']] * 2 + [recipe['details']['authored_armor']] * 2:
        raise RuntimeError('Affiliation handoff changed either authored health/armor')
    return handles


def validate_source(source, payload, recipe):
    saved = validate_switch_source(source, payload, recipe)
    handles = validate_exchange(source['probe'], source['final_probe'], A,
                               ((190, 210), (320, SAVE_FRAMES)), recipe)
    if source['extra']['rf_scene_vehicle_state'][12] != handles[B] or \
       source['final_probe']['rf_scene_apc_primary'][7] != saved['active_ammo']:
        raise RuntimeError('Source final sample lost saved active handle or ammunition')
    return saved


def validate(source, loaded, payload, recipe):
    validate_source(source, payload, recipe)
    result = validate_switch_save(source, loaded, payload, recipe)
    handles = validate_exchange(loaded['probe'], loaded['final_probe'], B,
                               ((40, 60), (110, LOAD_FRAMES)), recipe)
    if loaded['extra']['rf_scene_vehicle_state'][12] != handles[A] or \
       loaded['final_probe']['rf_scene_apc_primary'][7] != result['saved']['parked_ammo']:
        raise RuntimeError('Fresh-load final sample lost returned handle or saved ammunition')
    result.update(stored_affiliation_preserved=True,
        snapshots={label: dict(frame=sample['frame'], active=sample['active_owner'],
                               passive=sample['passive_owners'])
                   for label, sample in (
                       ('source_pre', source['probe']), ('source_final', source['final_probe']),
                       ('load_pre', loaded['probe']), ('load_final', loaded['final_probe']))},
        unchanged_save_formats=['RFSW1', 'RFVA2', 'RFVC3'], limitations=LIMITATIONS)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--resume-saved-run', type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        print(prepare_level(args.prepare_only)[0]); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT, ROOT / 'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT / 'artifacts/xemu' / ('vehicle-affiliation-' +
        datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture, recipe = prepare_level(folder / 'level')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'player-control.flag', 'scene-fixture.vpp', 'scene-preview.flag'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in names}
    report = dict(result='FAIL', phases={}, recipe=recipe)
    try:
        for name in names: (DISC / name).unlink(missing_ok=True)
        (DISC / 'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC / 'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64, b'\0') +
                                                  b'ctf06.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'scene-preview.flag').write_bytes(b'')
        (DISC / 'world-hdd-save.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior = args.resume_saved_run.resolve()
            source = json.loads((prior / 'save/result.json').read_text())
            payload = (prior / 'save/xbox-world.rfwc').read_bytes()
            report['resumed_source'] = str(prior)
        else:
            build(folder, 'save')
            source = run_guest(folder, 'save', hdd, SAVE_FRAMES, 600, capture_world=True,
                extra_symbols=SYMBOLS, probe=live_probe, probe_frame=190,
                final_probe=live_probe, final_probe_frame=320, allow_guest_error=True)
            payload = (folder / 'save/xbox-world.rfwc').read_bytes()
        report['phases']['save'] = source
        report['saved'] = validate_source(source, payload, recipe)
        (DISC / 'world-hdd-save.flag').unlink()
        (DISC / 'world-hdd-load.flag').write_bytes(b'1')
        (DISC / 'player-replay.bin').write_bytes(replay(True))
        build(folder, 'load')
        loaded = run_guest(folder, 'load', hdd, LOAD_FRAMES, 420, extra_symbols=SYMBOLS,
            probe=live_probe, probe_frame=40, final_probe=live_probe, final_probe_frame=110,
            allow_guest_error=True)
        report['phases']['load'] = loaded
        report.update(validate(source, loaded, payload, recipe))
    except Exception as exc:
        report['error'] = str(exc); raise
    finally:
        for name, data in original.items():
            if data is None: (DISC / name).unlink(missing_ok=True)
            else: (DISC / name).write_bytes(data)
        try: build(folder, 'restore')
        except Exception as exc:
            report['result'] = 'FAIL'; report['restore_error'] = str(exc); raise
        finally:
            report['disc_restored'] = all(
                ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
                for name, data in original.items())
            if not report['disc_restored']: report['result'] = 'FAIL'
            (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']: raise RuntimeError('Disc restoration failed')


if __name__ == '__main__': main()
