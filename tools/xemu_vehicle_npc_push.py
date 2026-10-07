"""Enemy-free, bounded stock-64-MiB Xbox passive-vehicle/NPC side pushes.

Keep original L20S2 geometry, Hanger Lift001 and unarmed friendly Eos4716.
The original When_Dead18354 setup output starts the lift; a fixture output
also activates original UnHide18359 for the retained hulls. Mode6 places the
idle NPC beside its actual moving Fighter4717 hull at frame40. Mode7 also
places a distinct stationary copy of that authored hull beyond the NPC.
Only the fixture UID, event links and unused controller callbacks change in
private level bytes. Runtime placement is explicit test setup, not a claim
of natural encounter placement. Contact/clearance/position telemetry is
read-only. No PC runtime, guest writes, images, host input or campaign route.

--prepare-only creates and independently decodes disposable archives without
building or running anything. The parent owns all builds, runs and cleanup.
"""

import argparse
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from inspect_moving_groups import inspect as inspect_groups
from xemu_guest_snapshot import words
from xemu_native_world_save import ROOT, DISC, FLAGS, address, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_turret_combat import entity_rows


ACTOR, PUSHER, BLOCKER = 4716, 4717, 915601
START_EVENT, SHOW_EVENT, LIFT_KEY = 18354, 18359, 4543
FRAMES, PLACE_FRAME, PROBE_FRAME = 75, 40, 45
NO_HANDLE = 0xffffffff
ROW_WORDS, HISTORY_ROWS = 36, 8
SYMBOLS = {
    'rf_scene_passive_npc_push': 8,
    'rf_scene_passive_npc_push_history': ROW_WORDS * HISTORY_ROWS,
    'rf_scene_passive_npc_push_last': ROW_WORDS,
    'rf_scene_passive_npc_blocker_fixture': 10,
    'rf_scene_passive_npc_blocker_separation': 4,
    'rf_scene_passive_roof_fixture': 10,
    'rf_scene_passive_npc_fixture': 10,
    'rf_scene_passive_attachment': 14,
    'rf_scene_passive_damage': 8,
    'rf_scene_passive_draw': 6,
    'rf_scene_vehicle_visibility': 8,
    'rf_scene_vehicle_visibility_history': 80,
    'rf_scene_enemy_combat': 8,
    'rf_scene_setup_result': 4,
}


def f(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def xyz(row, index):
    return [f(v) for v in row[index:index + 3]]


def norm(values):
    return math.sqrt(sum(v * v for v in values))


def section_payload(level, meta, kind):
    section = next(s for s in meta['sections'] if int(s['type'], 16) == kind)
    return level[section['offset'] + 8:section['offset'] + 8 + section['size']]


def event_rows(data):
    """Bounded v180 spans matching core/level.c; no executable/pefile needed."""
    at = 0

    def take(count):
        nonlocal at
        if count < 0 or count > len(data) - at:
            raise ValueError('Event record outside section')
        result = data[at:at + count]
        at += count
        return result

    def number():
        return struct.unpack('<I', take(4))[0]

    def string():
        return take(struct.unpack('<H', take(2))[0]).decode('cp1252')

    result = []
    for _ in range(number()):
        start = at
        uid, kind = number(), string()
        take(12)
        name = string()
        take(23)  # editor byte, delay, flags, two words and two float values
        string()
        string()
        links_at = at - start
        count = number()
        if count > (len(data) - at) // 4:
            raise ValueError('Event links outside record')
        links = [number() for _ in range(count)]
        links_end = at - start
        if kind.lower() in ('teleport', 'teleport_player', 'play_vclip', 'alarm'):
            take(36)
        take(4)
        result.append(dict(uid=uid, type=kind, name=name, links=links,
                           links_at=links_at, links_end=links_end,
                           raw=data[start:at]))
    if at != len(data):
        raise ValueError('Unconsumed event bytes')
    return result


def actor_details(row):
    """Read authored affiliation, weapons, vitals and creation/link flags."""
    data, at = row['raw'], row['transform'] + 48

    def string():
        nonlocal at
        if at + 2 > len(data):
            raise ValueError('Entity string length outside record')
        size = struct.unpack_from('<H', data, at)[0]
        at += 2
        value = data[at:at + size]
        at += size
        if len(value) != size:
            raise ValueError('Entity string outside record')
        return value.decode('cp1252')

    string()
    relationship, friendliness = struct.unpack_from('<2I', data, at + 1)
    at += 13
    string()
    string()
    health, armor = struct.unpack_from('<2f', data, at + 17)
    at += 29
    names = [string() for _ in range(7)]
    seat = struct.unpack_from('<i', data, at + 6)[0]
    at += 18
    creation = (2 if data[at + 1] else 0) | (4 if data[at + 15] else 0)
    return dict(relationship=relationship, friendliness=friendliness,
                health=health, armor=armor, primary=names[0], secondary=names[1],
                seat_host_uid=seat, creation_flags=creation)


def prepare_level(folder, blocked=False):
    source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L20S2.rfl'))
    if meta['version'] != 180 or meta['trailing_bytes']:
        raise ValueError('Expected complete original v180 L20S2')
    entities = entity_rows(source)
    actor = next(r for r in entities if r['uid'] == ACTOR)
    host = next(r for r in entities if r['uid'] == PUSHER)
    details = actor_details(actor)
    if actor['name'] != 'eos' or host['name'] != 'masako_fighter' or details != dict(
            relationship=1, friendliness=2, health=1., armor=0., primary='none',
            secondary='none', seat_host_uid=-1, creation_flags=4):
        raise ValueError(f'Original friendly/unarmed actor or hull changed: {details}')
    retained = [host['raw'], actor['raw']]
    if blocked:
        copy = bytearray(host['raw'])
        struct.pack_into('<I', copy, 0, BLOCKER)
        if copy[4:] != host['raw'][4:]:
            raise ValueError('Blocker copy changed more than its fixture UID')
        retained.append(bytes(copy))

    events = event_rows(section_payload(source, meta, 0x600))
    start = next(r for r in events if r['uid'] == START_EVENT)
    if start['type'] != 'When_Dead' or LIFT_KEY not in start['links']:
        raise ValueError('Original lift start output is absent')
    setup_links = [LIFT_KEY, SHOW_EVENT]
    setup = start['raw'][:start['links_at']] + U(len(setup_links), *setup_links) + start['raw'][start['links_end']:]
    show = next(r for r in events if r['uid'] == SHOW_EVENT)
    if show['type'] != 'UnHide' or PUSHER not in show['links']:
        raise ValueError('Original hidden-hull reveal event is absent')
    show_links = [PUSHER] + ([BLOCKER] if blocked else [])
    reveal = show['raw'][:show['links_at']] + U(len(show_links), *show_links) + show['raw'][show['links_end']:]

    group_data = section_payload(source, meta, 0x3000)
    groups = inspect_groups(group_data)
    group = next(g for g in groups if g['name'] == 'Hanger Lift001')
    if group['keys'][0]['uid'] != LIFT_KEY or PUSHER not in group['ids1'] or BLOCKER in group['ids1']:
        raise ValueError('Original Hanger Lift001 ownership changed')
    retained_group = bytearray(group_data[group['offset']:group['offset'] + group['bytes']])
    callbacks = []
    for key in group['keys']:
        links_at = key['offset'] - group['offset'] + key['bytes'] - 16
        if list(struct.unpack_from('<3I', retained_group, links_at)) != key['links']:
            raise ValueError('Independent moving-group link offset mismatch')
        for index, link in enumerate(key['links']):
            if link not in (0, NO_HANDLE):
                callbacks.append(dict(key_uid=key['uid'], slot=index, removed_event=link))
                struct.pack_into('<I', retained_group, links_at + 4 * index, NO_HANDLE)
    replacements = {0x30000: U(len(retained)) + b''.join(retained),
                    0x600: U(2) + setup + reveal, 0x3000: U(1) + retained_group,
                    0x60000: U(0), 0x20000: U(0), 0x10000: U(0)}
    out = bytearray(source[:meta['sections'][0]['offset']])
    offsets = {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = replacements.get(kind, section_payload(source, meta, kind))
        offsets[kind] = len(out)
        out += U(kind, len(payload)) + payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    checked = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L20S2.rfl'))
    if not checked['player_offset_matches'] or not checked['info_offset_matches'] or checked['trailing_bytes']:
        raise ValueError('Fixture section/header round trip failed')
    expected_uids = [PUSHER, ACTOR] + ([BLOCKER] if blocked else [])
    actual = entity_rows(out)
    if [r['uid'] for r in actual] != expected_uids or [r['raw'] for r in actual] != retained:
        raise ValueError('Fixture entity round trip changed authored records')
    checked_events = event_rows(section_payload(out, checked, 0x600))
    if [r['uid'] for r in checked_events] != [START_EVENT, SHOW_EVENT] or \
            checked_events[0]['links'] != setup_links or checked_events[1]['links'] != show_links:
        raise ValueError('Fixture retained an unrelated event')
    checked_groups = inspect_groups(section_payload(out, checked, 0x3000))
    if len(checked_groups) != 1 or checked_groups[0]['ids1'] != group['ids1'] or checked_groups[0]['ids2'] != group['ids2']:
        raise ValueError('Fixture altered original child membership')
    # Every section outside the documented minimal sanitizer is byte-exact,
    # including static geometry, lightmaps, mover brushes and source clutters.
    for section in meta['sections']:
        kind = int(section['type'], 16)
        if kind not in replacements and section_payload(out, checked, kind) != section_payload(source, meta, kind):
            raise ValueError(f'Unexpected source section edit: {kind:#x}')
    folder.mkdir(parents=True, exist_ok=True)
    fixture = folder / 'scene-fixture.vpp'
    archive(fixture, [('L20S2.rfl', out)])
    if read_entry(fixture, 'L20S2.rfl') != out:
        raise ValueError('Disposable VPP round trip failed')
    recipe = dict(
        status='PREPARED_NOT_RUN', source='levels2.vpp/L20S2.rfl', blocked=blocked,
        source_sha256=hashlib.sha256(source).hexdigest(),
        fixture_sha256=hashlib.sha256(out).hexdigest(), actor_uid=ACTOR,
        pusher_uid=PUSHER, blocker_uid=BLOCKER if blocked else None,
        entity_uids=expected_uids, removed_entities=len(entities) - 2,
        actor_details=details, source_actor_sha256=hashlib.sha256(actor['raw']).hexdigest(),
        source_host_sha256=hashlib.sha256(host['raw']).hexdigest(),
        source_start_links=start['links'], retained_start_links=setup_links,
        explicit_added_start_output=SHOW_EVENT, source_reveal_links=show['links'],
        retained_reveal_links=show_links,
        retained_group=group['name'], cleared_key_callbacks=callbacks,
        removed_events=len(events) - 2, removed_groups=len(groups) - 1,
        original_triggers=struct.unpack_from('<I', section_payload(source, meta, 0x60000))[0],
        frames=FRAMES, placement_frame=PLACE_FRAME, fixture_mode=7 if blocked else 6,
        preserved_sections='All sections except entities/events/moving_groups/triggers/nav_points/nav_paths; original geometry, movers and clutters are byte-exact.',
        limitations='Explicit idle-NPC and optional blocker placement. Translated attached hull only; no campaign encounter, rotation, crush damage, animation or save/load claim.')
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    return fixture, recipe


def live_probe(monitor, mapping):
    result = {name: words(monitor, address(mapping, name), count) for name, count in SYMBOLS.items()}
    result['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return result


def contact_rows(extra):
    hits = extra['rf_scene_passive_npc_push'][1]
    raw = extra['rf_scene_passive_npc_push_history']
    rows = [raw[i * ROW_WORDS:(i + 1) * ROW_WORDS] for i in range(min(hits, HISTORY_ROWS))]
    last = extra['rf_scene_passive_npc_push_last']
    if hits and (not rows or last != rows[-1]):
        rows.append(last)
    return rows


def validate_contact(row, actor_handle, pusher_handle, blocker_handle):
    if len(row) != ROW_WORDS or not PLACE_FRAME <= row[0] < FRAMES:
        raise RuntimeError(f'Contact outside bounded placement window: {row}')
    if row[1:5] != [ACTOR, actor_handle, PUSHER, pusher_handle] or row[31] != 1:
        raise RuntimeError(f'Contact lost exact actor/pusher registration: {row}')
    if not actor_handle or not pusher_handle or actor_handle == pusher_handle or NO_HANDLE in (actor_handle, pusher_handle):
        raise RuntimeError('Contact handles are invalid or aliased')
    if row[29:31] != [NO_HANDLE, NO_HANDLE] or row[27] != row[28] or row[35] & (2 | 8 | 0x4000):
        raise RuntimeError('Push changed seating/support or admitted an ineligible NPC')
    floats = [f(v) for v in row[6:23] + row[25:27] + row[32:35]]
    if not all(math.isfinite(v) for v in floats) or f(row[25]) != 1. or row[25] != row[26]:
        raise RuntimeError('Push changed original living health or produced nonfinite telemetry')
    contact, clearance = f(row[6]), f(row[7])
    if not 0 <= contact <= 1 or not 0 <= clearance <= 1 or row[23] not in (0, 1) or row[24] not in (0, 1):
        raise RuntimeError('Invalid contact/clearance fraction or result flags')
    before, proposed, after = xyz(row, 8), xyz(row, 11), xyz(row, 14)
    host_before, host_after = xyz(row, 17), xyz(row, 20)
    delta = [host_after[i] - host_before[i] for i in range(3)]
    wanted = [proposed[i] - before[i] for i in range(3)]
    actual = [after[i] - before[i] for i in range(3)]
    if norm(delta) <= 1e-6 or norm(delta) > .25 or norm(wanted) > norm(delta) + .002:
        raise RuntimeError('Push proposal is not bounded by the real chassis translation')
    if sum(wanted[i] * delta[i] for i in range(3)) < -1e-6 or norm(actual) > norm(wanted) + .002:
        raise RuntimeError('NPC moved backward or teleported past the proposed push')
    if any(abs(after[i] - (before[i] + wanted[i] * clearance)) > .002 for i in range(3)):
        raise RuntimeError('NPC displacement does not match accepted clearance fraction')
    if any(abs(f(row[32 + i]) - after[i]) > .0002 for i in range(3)):
        raise RuntimeError('Committed NPC position was not published to its live view')
    if row[23] and (not blocker_handle or row[5] != blocker_handle or row[5] == pusher_handle or clearance >= 1):
        raise RuntimeError(f'Clearance did not preserve the distinct blocking owner: {row}')
    return dict(frame=row[0], contact_fraction=contact, clearance_fraction=clearance,
                before=before, proposed=proposed, after=after,
                host_delta=delta, accepted_distance=norm(actual), blocked=bool(row[23]))


def validate(guest, recipe):
    if guest['guest_phase'] != 5 or guest['frames'] != FRAMES or guest['memory_bytes'] != 64 * 1024 * 1024 or guest['free_pages'] <= 0:
        raise RuntimeError('Incomplete bounded stock-64-MiB Xbox run')
    early, end = guest['probe'], guest['extra']
    if not PLACE_FRAME <= early['frame'] < FRAMES or guest['player_life'][2]:
        raise RuntimeError('Live probe missed the push window or player died')
    fixture, npc = end['rf_scene_passive_roof_fixture'], end['rf_scene_passive_npc_fixture']
    attached, push = end['rf_scene_passive_attachment'], end['rf_scene_passive_npc_push']
    if fixture[:3] != [PUSHER, PLACE_FRAME, 1] or fixture[9] != recipe['fixture_mode']:
        raise RuntimeError(f'Expected exactly one explicit NPC side placement: {fixture}')
    if attached[:2] != [1, 1] or attached[2] < 1 or attached[3] or attached[4] != PUSHER or attached[13]:
        raise RuntimeError(f'Original attached Fighter/lift ownership failed: {attached}')
    if end['rf_scene_setup_result'] != [1, START_EVENT, 16, 0]:
        raise RuntimeError('Expected one ordinary When_Dead setup activation')
    if push[0] < push[1] or not push[1] or push[7] or (not push[2] and not recipe['blocked']):
        raise RuntimeError(f'Missing accepted moving-hull NPC contacts: {push}')
    if not npc[0] or npc[0] != early['rf_scene_passive_npc_fixture'][0] or npc[5] or f(npc[6]) != 1.:
        raise RuntimeError('Idle NPC identity, original health or stopped script did not persist')
    if push[4:6] != [npc[0], attached[5]]:
        raise RuntimeError('Final push counters no longer identify the same actor and pusher')
    for sample in (early, end):
        combat, damage = sample['rf_scene_enemy_combat'], sample['rf_scene_passive_damage']
        if combat[2] or combat[3] or combat[7] or damage[2] or damage[3] or damage[6]:
            raise RuntimeError('Unexpected combat, vehicle damage or runtime error in enemy-free fixture')
    blocker = end['rf_scene_passive_npc_blocker_fixture']
    blocker_handle = 0
    if recipe['blocked']:
        separation = end['rf_scene_passive_npc_blocker_separation']
        if not separation[0] or separation[3] or not math.isfinite(f(separation[1])) or f(separation[1]) < .14:
            raise RuntimeError(f'Blocker fixture begins inside an authored hull sphere: {separation}')
        if blocker[:2] != [1, BLOCKER] or not blocker[2] or blocker[9] or blocker[3:6] != blocker[6:9]:
            raise RuntimeError(f'Distinct stationary blocker placement/identity failed: {blocker}')
        blocker_handle = blocker[2]
        if blocker_handle in (npc[0], attached[5], NO_HANDLE) or not push[3]:
            raise RuntimeError('Blocker aliases actor/pusher or no blocked contact occurred')
    elif any(blocker) or push[3]:
        raise RuntimeError('Open fixture unexpectedly contains a blocker or blocked push')
    expected_owners = {PUSHER: attached[5]}
    if recipe['blocked']:
        expected_owners[BLOCKER] = blocker_handle
    if end['rf_scene_passive_damage'][7] != len(expected_owners):
        raise RuntimeError('Unexpected passive-owner population in isolated fixture')
    visibility = end['rf_scene_vehicle_visibility']
    if visibility[:4] != [len(expected_owners), len(expected_owners), 0, 0] or visibility[7]:
        raise RuntimeError(f'Original UnHide did not reveal exactly the retained hulls: {visibility}')
    raw_visibility = end['rf_scene_vehicle_visibility_history']
    revealed = set()
    for index in range(len(expected_owners)):
        row = raw_visibility[index * 10:(index + 1) * 10]
        if row[0] >= PLACE_FRAME or row[1] in revealed or expected_owners.get(row[1]) != row[2] or \
                row[3] != 1 or not row[4] & 0x4000 or row[5] != row[4] & ~0x4000 or \
                f(row[6]) <= 0 or row[7] != 1:
            raise RuntimeError(f'Hull reveal did not preserve a living exact owner before placement: {row}')
        revealed.add(row[1])
    draw = end['rf_scene_passive_draw']
    if draw[0] != len(expected_owners) or draw[1] < 1 or draw[2] < 1 or draw[5]:
        raise RuntimeError(f'Visible retained hull was not submitted normally: {draw}')
    rows = contact_rows(end)
    contacts = [validate_contact(row, npc[0], attached[5], blocker_handle) for row in rows]
    if recipe['blocked'] and not any(row['blocked'] for row in contacts):
        raise RuntimeError('Saved contact history does not establish blocked clearance')
    initial, final = xyz(fixture, 3), xyz(npc, 2)
    displacement = final[0] - initial[0]
    host_move = f(attached[7]) - f(fixture[6])
    if not all(math.isfinite(v) for v in initial + final) or displacement * host_move < -1e-5:
        raise RuntimeError('NPC final displacement opposes its pusher')
    if abs(displacement) > abs(host_move) + .10:
        raise RuntimeError('NPC displacement exceeds post-placement host travel')
    if not recipe['blocked'] and abs(displacement) < .15:
        raise RuntimeError('Open case lacks measurable identity-preserving NPC displacement')
    return dict(result='PASS', actor_uid=ACTOR, actor_handle=npc[0],
                pusher_uid=PUSHER, pusher_handle=attached[5],
                blocker_uid=BLOCKER if recipe['blocked'] else None,
                blocker_handle=blocker_handle, hits=push[1], moves=push[2], blocked=push[3],
                initial_position=initial, final_position=final,
                x_displacement=displacement, pusher_x_displacement=host_move,
                contacts=contacts, free_pages=guest['free_pages'],
                limitations=recipe['limitations'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path, help='Create/verify fixture files only; no disc changes, build or emulator')
    parser.add_argument('--case', choices=('all', 'open', 'blocked'), default='all')
    args = parser.parse_args()
    cases = ('open', 'blocked') if args.case == 'all' else (args.case,)
    if args.prepare_only:
        for name in cases:
            path, _ = prepare_level(args.prepare_only / name, name == 'blocked')
            print(path, flush=True)
        return
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('vehicle-npc-push-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {
        'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in names}
    report = dict(result='FAIL', scope=__doc__, cases={})
    try:
        for name in cases:
            case_folder = folder / name
            fixture, recipe = prepare_level(case_folder / 'level', name == 'blocked')
            case_report = dict(recipe=recipe)
            report['cases'][name] = case_report
            for flag in names:
                (DISC / flag).unlink(missing_ok=True)
            (DISC / 'scene-fixture.vpp').write_bytes(fixture.read_bytes())
            (DISC / 'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'))
            for flag in ('scene-preview.flag', 'player-control.flag', 'campaign-spawn.flag'):
                (DISC / flag).write_bytes(b'')
            (DISC / 'campaign-actor.bin').write_bytes(U(PUSHER))
            (DISC / 'campaign-setup.bin').write_bytes(U(START_EVENT))
            (DISC / 'campaign-passive-roof.bin').write_bytes(U(PUSHER, PLACE_FRAME, recipe['fixture_mode']))
            (DISC / 'player-replay.bin').write_bytes(b'RFI6' + U(48) + bytes(FRAMES * 48))
            build(case_folder, name)
            case_report['binary_sha256'] = {}
            for label, source in (('default.xbe', DISC / 'default.xbe'),
                                  ('main.exe', ROOT / 'build/xbox/main.exe'),
                                  ('main.map', ROOT / 'build/xbox/main.map')):
                data = source.read_bytes()
                case_report['binary_sha256'][label] = hashlib.sha256(data).hexdigest()
                (case_folder / label).write_bytes(data)
            guest = run_guest(case_folder, 'run', hdd, FRAMES, 420, snapshot=True,
                              extra_symbols=SYMBOLS, probe=live_probe, probe_frame=PROBE_FRAME,
                              allow_guest_error=True)
            case_report['guest'] = guest
            case_report['validation'] = validate(guest, recipe)
        if len(cases) == 2:
            open_move = abs(report['cases']['open']['validation']['x_displacement'])
            blocked_move = abs(report['cases']['blocked']['validation']['x_displacement'])
            if blocked_move >= open_move - .05:
                raise RuntimeError(f'Blocker did not materially limit displacement: open={open_move}, blocked={blocked_move}')
            report['displacement_reduction'] = open_move - blocked_move
        report['result'] = 'PASS'
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        try:
            build(folder, 'restore')
        except Exception as exc:
            report['result'] = 'FAIL'
            report['restore_build_error'] = str(exc)
            raise
        finally:
            report['disc_restored'] = all(((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
                                          for name, data in original.items())
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Test disc restoration failed')


if __name__ == '__main__':
    main()
