"""Bounded stock-64-MiB Xbox checks for authored moving vehicle controls.

Private L20S2 retains Fighter4717, lamps4527/4528, switch clutter4529 and
Use trigger4530 with its real links[4529,4543]. Auto trigger18344 is removed.
All geometry, entity and clutter records stay byte-exact. Trigger4530 is
explicitly enabled and its30s cooldown removed in fixture bytes; its2m box
and links are unchanged. A retained Invert17967 is retargeted to4530 to save
a genuinely disabled moving trigger. Original UnHide18359 reveals Fighter.
Translation and yaw controllers use explicit6-second legs; yaw is120 degrees
about the original first-key +Y axis. Arrival callbacks are removed.

The opt-in C hook submits explicit positions to the ordinary scene contact/
Use callback, without relocating the player. It tests authored old center,
current center without Use, current center with Use, and disabled rejection.
Fresh process load observes saved flags/counts and controller phase plus one
ordinary tick, then enables via the ordinary linked-trigger callback and
tests further contact/motion. This does not prove natural player approach,
switch-model animation, physical input, general child families or retail parity.

--prepare-only writes disposable fixture/recipe only. The parent owns builds,
runtime staging, serial XEMU runs and cleanup. No PC runtime, guest memory
writes, screenshots, host input, user PC or campaign route is used.
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
from inspect_clutter_records import inspect as inspect_clutter
from inspect_levels import inspect
from inspect_moving_groups import inspect as inspect_groups
from inspect_triggers import inspect as inspect_triggers
from xemu_guest_snapshot import words
from xemu_native_world_save import ROOT, DISC, FLAGS, address, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import actor_details, event_rows, section_payload
from xemu_vehicle_rotating_support import (checkpoint_sections, close_vector,
    decode_group_spans, preserve_launch_binaries, replay, replace_links)

HOST, KEY, TRIGGER, SHOW, DISABLE = 4717, 4543, 4530, 18359, 17967
CHILDREN = [4527, 4528, 4529, TRIGGER]
NO_HANDLE = 0xffffffff
FRAMES, LOAD_FRAMES, DURATION, YAW_DEGREES = 150, 40, 6., 120.
SYMBOLS = {
    'rf_scene_group_control_fixture': 16,
    'rf_scene_group_control_fixture_rows': 8 * 128,
    'rf_scene_group_control_fixture_children': 8 * 4 * 64,
    'rf_scene_group_control_children': 12,
    'rf_scene_group_control_child_rows': 16 * 64,
    'rf_scene_passive_attachment': 14,
    'rf_scene_passive_damage': 8,
    'rf_scene_setup_result': 4,
    'rf_scene_trigger_contacts': 6,
    'rf_scene_live_activation': 8,
    'rf_scene_rotating_doors': 8,
    'rf_scene_live_motion': 8,
    'rf_scene_enemy_combat': 8,
    'rf_scene_world_load_reject': 3,
    'rf_scene_checkpoint_world_reject': 9,
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def f(word):
    return struct.unpack('<f', U(word))[0]


def vector(row, offset, size=3):
    return [f(v) for v in row[offset:offset + size]]


def norm(values):
    return math.sqrt(sum(v * v for v in values))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def prepare_group(payload, source, mode):
    raw = payload[source['offset']:source['offset'] + source['bytes']]
    checked = decode_group_spans(raw)
    require(checked['name'] == 'Hanger Lift001' and checked['flags'] == [1, 0, 0, 0, 0, 0]
        and checked['mode'] == 2 and checked['ids1'] == [4527, 4528, 4529, 4530, 18344, 4717]
        and checked['ids2'] == [4531], 'Original Hanger Lift001 metadata changed')
    for first, second in zip(source['keys'], checked['keys']):
        require(first['offset'] - source['offset'] == second['offset'] and
            all(first[k] == second[k] for k in ('bytes', 'uid', 'position',
                'orientation_disk', 'timing', 'links', 'rotation')),
            'Independent group readers disagree')
    out, changes = bytearray(raw), []

    def patch(field, offset, data):
        prior = bytes(out[offset:offset + len(data)])
        require(len(prior) == len(data), 'Group patch outside record')
        out[offset:offset + len(data)] = data
        changes.append(dict(field=field, offset=offset, before_hex=prior.hex(), after_hex=data.hex()))

    for key in checked['keys']:
        patch('key%d.timing' % key['uid'], key['timing_at'], struct.pack('<5f', 0, DURATION, DURATION, 0, 0))
        patch('key%d.callbacks' % key['uid'], key['links_at'], U(NO_HANDLE, NO_HANDLE, NO_HANDLE))
    patch('one_way_mode', checked['mode_at'], U(1))
    if mode == 'yaw':
        patch('rotation_flag', checked['flags_at'] + 1, b'\1')
        patch('key4543.rotation_degrees', checked['keys'][0]['rotation_at'], struct.pack('<f', YAW_DEGREES))
    ids_at = len(raw) - (8 + 4 * (len(checked['ids1']) + len(checked['ids2'])))
    require(raw[ids_at:] == U(len(checked['ids1']), *checked['ids1'], len(checked['ids2']), *checked['ids2']),
            'Independent group membership span mismatch')
    ids = CHILDREN + [HOST]
    changes.append(dict(field='first_list_remove_absent_auto18344', offset=ids_at,
        before_hex=raw[ids_at:].hex(), after_hex=U(len(ids), *ids, 1, 4531).hex()))
    out = out[:ids_at] + U(len(ids), *ids, 1, 4531)
    result = decode_group_spans(out)
    require(result['ids1'] == ids and result['ids2'] == [4531] and result['mode'] == 1,
            'Private group membership or mode failed round trip')
    for field in ('name', 'header', 'legacy', 'unknown', 'sounds'):
        require(result[field] == checked[field], 'Unrelated group field changed: ' + field)
    for index, key in enumerate(result['keys']):
        require(key['position'] == checked['keys'][index]['position'] and
                key['orientation_disk'] == checked['keys'][index]['orientation_disk'] and
                key['timing'] == [0., DURATION, DURATION, 0., 0.] and key['links'] == [NO_HANDLE] * 3,
                'Group pose/timing/callback round trip mismatch')
    return bytes(out), dict(edits=changes, decoded=result,
        note='Legacy editor relative-pose rows remain untouched; absent18344 has no live record or first-list membership.')


def prepare_trigger(payload, source):
    raw = payload[source['offset']:source['offset'] + source['bytes']]
    require(source['uid'] == TRIGGER and source['shape'] == 1 and source['flags'] == [1, 0, 0, 0, 0]
            and source['links'] == [4529, KEY] and source['dimensions_disk'] == [2., 2., 2.]
            and source['tail_flag'] == 1 and source['timing'] == 30. and source['values'][1] == 0.,
            'Original trigger4530 semantics changed')
    # Independent offsets from bounded v180 field sequence and the fixed tail.
    name_length = struct.unpack_from('<H', raw, 4)[0]
    timing_at = 4 + 2 + name_length + 1 + 4
    tail_at = len(raw) - (1 + 8 + 4 + 4 + 4 * len(source['links']))
    require(struct.unpack_from('<f', raw, timing_at)[0] == 30. and raw[tail_at] == 1,
            'Trigger field-span disagreement')
    out = bytearray(raw)
    struct.pack_into('<f', out, timing_at, 0.)
    out[tail_at] = 0
    result = inspect_triggers(U(1) + out)[0]
    for key in source:
        if key not in ('offset', 'timing', 'tail_flag'):
            require(result[key] == source[key], 'Unrelated trigger field changed: ' + key)
    return bytes(out), [dict(field='cooldown_seconds', offset=timing_at, before=30., after=0.),
                        dict(field='initial_disabled', offset=tail_at, before=1, after=0)]


def prepare_level(folder, mode='translation'):
    require(mode in ('translation', 'yaw'), 'Unknown control fixture mode')
    source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L20S2.rfl'))
    require(meta['version'] == 180 and not meta['trailing_bytes'], 'Expected complete v180 L20S2')
    host = next(r for r in entity_rows(source) if r['uid'] == HOST)
    require(host['name'] == 'masako_fighter' and actor_details(host)['health'] == 2000.,
            'Original Fighter4717 class/vitals changed')
    clutter_data = section_payload(source, meta, 0x50000)
    clutter = [next(r for r in inspect_clutter(clutter_data) if r['uid'] == uid) for uid in CHILDREN[:3]]
    require([r['class_name'] for r in clutter] == [b'Mine Light 8', b'Mine Light 8', b'2PartSwitch02'],
            'Original attached clutter classes changed')
    clutter_raw = [clutter_data[r['offset']:r['offset'] + r['bytes']] for r in clutter]
    trigger_data = section_payload(source, meta, 0x60000)
    trigger = next(r for r in inspect_triggers(trigger_data) if r['uid'] == TRIGGER)
    retained_trigger, trigger_edits = prepare_trigger(trigger_data, trigger)
    events = event_rows(section_payload(source, meta, 0x600))
    show = next(r for r in events if r['uid'] == SHOW)
    disable = next(r for r in events if r['uid'] == DISABLE)
    require(show['type'] == 'UnHide' and HOST in show['links'] and disable['type'] == 'Invert',
            'Expected original reveal/Invert event')
    retained_events = [replace_links(show, [HOST]), replace_links(disable, [TRIGGER])]
    # Invert must be immediate; this is an independently checked original field.
    delay_at = 4 + 2 + len(disable['type']) + 12 + 2 + len(disable['name']) + 1
    require(struct.unpack_from('<f', disable['raw'], delay_at)[0] == 0., 'Original Invert is delayed')
    group_data = section_payload(source, meta, 0x3000)
    group = next(r for r in inspect_groups(group_data) if r['name'] == 'Hanger Lift001')
    retained_group, controller = prepare_group(group_data, group, mode)
    replacements = {0x30000: U(1) + host['raw'], 0x50000: U(3) + b''.join(clutter_raw),
        0x60000: U(1) + retained_trigger, 0x600: U(2) + b''.join(retained_events),
        0x3000: U(1) + retained_group, 0x20000: U(0), 0x10000: U(0)}
    out, offsets = bytearray(source[:meta['sections'][0]['offset']]), {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        data = replacements.get(kind, section_payload(source, meta, kind))
        offsets[kind] = len(out)
        out += U(kind, len(data)) + data
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    check = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L20S2.rfl'))
    require(check['player_offset_matches'] and check['info_offset_matches'] and not check['trailing_bytes'],
            'Private RFL header/section offsets failed round trip')
    actual = entity_rows(out)
    require(len(actual) == 1 and actual[0]['raw'] == host['raw'], 'Fighter bytes changed')
    actual_clutter = section_payload(out, check, 0x50000)
    require([r['uid'] for r in inspect_clutter(actual_clutter)] == CHILDREN[:3] and
            actual_clutter == U(3) + b''.join(clutter_raw), 'Clutter bytes changed')
    require([r['uid'] for r in inspect_triggers(section_payload(out, check, 0x60000))] == [TRIGGER],
            'Unexpected trigger or retained auto18344')
    actual_events = event_rows(section_payload(out, check, 0x600))
    require([(r['uid'], r['links']) for r in actual_events] == [(SHOW, [HOST]), (DISABLE, [TRIGGER])],
            'Retained event links failed round trip')
    unchanged = []
    for section in meta['sections']:
        kind = int(section['type'], 16)
        if kind not in replacements:
            before, after = section_payload(source, meta, kind), section_payload(out, check, kind)
            require(before == after, 'Unrelated section changed: ' + hex(kind))
            unchanged.append(dict(type=hex(kind), bytes=len(before), sha256=sha(before)))
    children = []
    for record, raw in zip(clutter, clutter_raw):
        disk = list(struct.unpack('<9f', record['matrix']))
        children.append(dict(uid=record['uid'], kind=4, class_name=record['class_name'].decode('cp1252'),
            position=list(struct.unpack('<3f', record['position'])), basis=disk,
            record_sha256=sha(raw)))
    disk = trigger['orientation_disk']
    children.append(dict(uid=TRIGGER, kind=5, position=trigger['position'], basis=disk[3:] + disk[:3],
        dimensions=[2., 2., 2.], links=trigger['links'], record_sha256=sha(retained_trigger)))
    host_pose = list(struct.unpack_from('<12f', host['raw'], host['transform']))
    folder.mkdir(parents=True, exist_ok=True)
    fixture = folder / 'scene-fixture.vpp'
    require(not fixture.exists() or fixture.stat().st_nlink == 1, 'Refusing linked fixture archive replacement')
    archive(fixture, [('L20S2.rfl', out)])
    require(read_entry(fixture, 'L20S2.rfl') == out, 'Private archive round trip failed')
    recipe = dict(status='PREPARED_NOT_RUNTIME_VALIDATED', mode=mode, source_level='L20S2.rfl',
        source_sha256=sha(source), level_sha256=sha(out), archive_sha256=sha(fixture.read_bytes()),
        controller=controller, trigger_edits=trigger_edits, children=children,
        host=dict(uid=HOST, health=2000., position=host_pose[:3], basis=host_pose[6:] + host_pose[3:6],
                  record_sha256=sha(host['raw'])),
        source_frames=[0, 5, 110, 111, 112, 140, 141, 148], source_operations=[0, 1, 2, 3, 4, 5, 6, 0],
        load_frames=[1, 2, 3, 4, 5, 6, 20, 38], load_operations=[0, 6, 7, 2, 3, 4, 0, 0],
        event_edits=[dict(uid=SHOW, links_before=show['links'], links_after=[HOST]),
                     dict(uid=DISABLE, links_before=disable['links'], links_after=[TRIGGER])],
        unchanged_sections=unchanged, limitations=__doc__)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    return fixture, recipe


def source_inputs(fixture, recipe):
    return {'scene-fixture.vpp': fixture.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'),
        'scene-preview.flag': b'', 'campaign-spawn.flag': b'', 'player-control.flag': b'',
        'campaign-actor.bin': U(HOST), 'campaign-setup.bin': U(SHOW),
        'campaign-attached-controls.bin': U(0x54434652, 1, FRAMES, 0),
        'player-replay.bin': replay(FRAMES), 'world-hdd-save.flag': b'1'}


def live_probe(monitor, mapping):
    return {name: words(monitor, address(mapping, name), count) for name, count in SYMBOLS.items()}


def rotate(point, axis, angle):
    length = norm(axis)
    require(length > 1e-9, 'Zero controller rotation axis')
    axis = [v / length for v in axis]
    c, s = math.cos(angle), math.sin(angle)
    dot = sum(axis[i] * point[i] for i in range(3))
    return [point[i] * c + axis[i] * dot * (1 - c) +
        (axis[(i + 1) % 3] * point[(i + 2) % 3] - axis[(i + 2) % 3] * point[(i + 1) % 3]) * s for i in range(3)]


def expected_pose(position, basis, child):
    origin, pending, axis = vector(child, 46), vector(child, 49), vector(child, 52)
    if child[45] & 4:
        angle = f(child[55]) * (-1 if f(child[56]) < 0 else 1)
        result = rotate([position[i] - origin[i] for i in range(3)], axis, angle)
        result = [result[i] + origin[i] for i in range(3)]
        rotated = sum((rotate(basis[i:i + 3], axis, angle) for i in (0, 3, 6)), [])
        return result, rotated
    return [position[i] + pending[i] - origin[i] for i in range(3)], basis


def decode_rows(extra):
    raw, children = extra['rf_scene_group_control_fixture_rows'], extra['rf_scene_group_control_fixture_children']
    return [(raw[i * 128:(i + 1) * 128], [children[(i * 4 + j) * 64:(i * 4 + j + 1) * 64]
            for j in range(4)]) for i in range(8)]


def validate_pose(row, child_rows, recipe):
    require(row[3] == 0 and row[107:109] == [1, 1] and row[109:112] == [31, 31, 31],
            'A contact status, exact owner identity or clutter publication failed')
    require(row[99] == 1 and row[100] == 0 and row[102] == NO_HANDLE,
            'Trigger shape/cooldown/activation limit changed')
    close_vector(vector(row, 96), [2., 2., 2.], 0, 'actual trigger box size')
    require(row[103] == 1 and row[104] == row[89] and row[105] in (1, 2) and row[106] == row[16],
            'Real trigger links lost exact switch/controller owners')
    for child, original in zip(child_rows, recipe['children']):
        require(child[0] == original['uid'] and child[1] == original['kind'] and child[4:6] == [1, 1]
                and child[44] == row[16] and child[58:60] == [0, 0],
                'Authored child membership, registry identity or read failed')
        close_vector(vector(child, 8), original['position'], 0.00002, 'immutable authored child position')
        close_vector(vector(child, 11, 9), original['basis'], 0.000002, 'immutable authored child basis')
        position, basis = expected_pose(original['position'], original['basis'], child)
        for offset in (20, 32):
            close_vector(vector(child, offset), position, .0002, 'child transformed center')
        for offset in (23, 35):
            close_vector(vector(child, offset, 9), basis, .000003, 'child all transformed axes')
        require(child[20:32] == child[32:44], 'Adapter pose differs from published owner')
    trigger = child_rows[-1]
    require(row[57:69] == trigger[32:44], 'Contact used a different trigger pose than publication')
    host = recipe['host']
    position, basis = expected_pose(host['position'], host['basis'], trigger)
    close_vector(vector(row, 72), position, .0003, 'Fighter/control coherent center')
    close_vector(vector(row, 75, 9), basis, .000003, 'Fighter/control coherent basis')
    require(f(row[71]) == host['health'], 'Fighter health changed')
    return dict(frame=row[0], operation=row[1], count_before=row[6], count_after=row[7],
        flags_before=row[8], flags_after=row[9], phase=f(row[21]), distance=f(row[23]),
        controller_position=vector(row, 24), center=vector(row, 57), basis=vector(row, 60, 9),
        fighter_position=vector(row, 72), trigger_handle=row[5], fighter_handle=row[69],
        clutter_handles=row[87:90], clutter_health=vector(row, 84))


def validate_phase(guest, recipe, loaded=False, saved=None):
    frames, phase = (LOAD_FRAMES, 2) if loaded else (FRAMES, 1)
    require(guest['guest_phase'] == 5 and guest['frames'] == frames and guest['memory_bytes'] == 64 * 1024 * 1024
            and guest['free_pages'] > 0 and not guest['player_life'][2], 'Incomplete live stock64MiB phase')
    extra, summary = guest['extra'], guest['extra']['rf_scene_group_control_fixture']
    require(summary[:7] == [1, phase, 8, 4 if loaded else 5, 0 if loaded else 1, 1 if loaded else 0, 0],
            'Fixture did not perform the exact ordinary callbacks: %r' % summary)
    adapter = extra['rf_scene_group_control_children']
    require(adapter[0:2] == [4, 4] and adapter[3] > 1 and adapter[4] > 4 and adapter[5] > 0
            and adapter[6:8] == [0, 0] and adapter[9] == 0 and adapter[10] == 4,
            'Live four-owner adapter failed: %r' % adapter)
    require(extra['rf_scene_passive_attachment'][13] == 0, 'Passive host attachment reported errors')
    observations, identities, health = [], None, None
    frames_expected = recipe['load_frames' if loaded else 'source_frames']
    ops_expected = recipe['load_operations' if loaded else 'source_operations']
    pairs = decode_rows(extra)
    for index, (row, children) in enumerate(pairs):
        require(row[:2] == [frames_expected[index], ops_expected[index]], 'Missing exact scheduled sample')
        observation = validate_pose(row, children, recipe)
        ids = [row[5], row[16], row[69], *row[87:90], row[90]]
        require(all(h not in (0, NO_HANDLE) for h in ids) and len(set(ids)) == len(ids),
                'Distinct full generation handles missing')
        if identities is None:
            identities, health = ids, row[84:87]
        require(ids == identities and row[84:87] == health, 'Owner identity or clutter health changed within phase')
        delta, fired, ready = row[7] - row[6], row[13] - row[12], row[15] - row[14]
        expected = int(row[1] in (1, 4))
        require((delta, fired, ready) == (expected, expected, expected),
                'Ordinary old/no-Use/current/disabled acceptance mismatch at frame%d: %r' %
                (row[0], (delta, fired, ready)))
        if row[1] == 2:
            close_vector(vector(row, 30), vector(row, 45), 0, 'old-center callback position')
            # Each2m box has circumscribed radius sqrt3; separation>4 excludes
            # both entire boxes and the ordinary <=1m Use reach extension.
            require(norm([f(row[33 + i]) - f(row[45 + i]) for i in range(3)]) > 4.,
                    'Old/current boxes are not strictly separated')
        if row[1] in (1, 3, 4, 6):
            close_vector(vector(row, 30), vector(row, 33), 0, 'current-center callback position')
        if row[1] == 5:
            require(not row[8] & 16 and row[9] & 16, 'Ordinary Invert did not disable tested trigger')
        if row[1] == 6:
            require(row[8] & 16 and row[9] & 16, 'Disabled contact was not made while disabled')
        if row[1] == 7:
            require(row[8] & 16 and not row[9] & 16, 'Ordinary linked enable did not clear disabled bit')
        if loaded:
            require(row[113] == 0, 'Fresh process replayed campaign setup')
            require(row[84:87] == saved['clutter_health_bits'] and ids == saved['handles'],
                    'Fresh process changed full handles or saved clutter health')
        observations.append(observation)
    first, last = pairs[0][0], pairs[-1][0]
    require(norm([f(last[57 + i]) - f(first[57 + i]) for i in range(3)]) > (.15 if loaded else 4.),
            'Control child did not continue moving')
    if loaded:
        require(guest['checkpoint_state'][8] == 1 and not guest['checkpoint_state'][0]
                and not any(extra['rf_scene_setup_result']), 'Ordinary fresh-load failed or replayed setup')
        require(first[6:8] == [saved['count'], saved['count']] and (first[8] & ~64) == saved['flags']
                and first[10] == saved['object_flags'], 'Loaded activation bookkeeping did not reconstruct')
        close_vector([f(first[20])], [saved['controller_phase'] + 1 / 60], .00002,
                     'restored phase plus one ordinary tick')
        if recipe['mode'] == 'yaw':
            close_vector([f(first[22])], [saved['controller_distance'] - math.radians(YAW_DEGREES) / DURATION / 60],
                         .000003, 'restored yaw plus one ordinary tick')
        else:
            keys = recipe['controller']['decoded']['keys']
            direction = [keys[1]['position'][i] - keys[0]['position'][i] for i in range(3)]
            length = norm(direction)
            close_vector(vector(first, 24), [saved['controller_pending'][i] +
                         direction[i] / length * saved['controller_speed'] / 60 for i in range(3)],
                         .00015, 'restored translation plus one ordinary tick')
        require(last[7] == 3 and not last[9] & 16, 'Fresh Use did not produce exactly one new activation')
    else:
        require(first[6:8] == [0, 0] and last[7] == 2 and last[9] & 16,
                'Source did not finish with two activations and ordinary disabled state')
        require(extra['rf_scene_setup_result'][0:2] == [1, SHOW] and extra['rf_scene_setup_result'][3] == 0,
                'Source did not perform exactly one ordinary Fighter reveal')
    return dict(result='PASS', free_pages=guest['free_pages'], observations=observations,
                handles=identities, clutter_health_bits=health)


def component(data, magic, version):
    require(len(data) >= 64 and data[:4] == magic and struct.unpack_from('<2I', data, 4) == (version, len(data)),
            'Unexpected unchanged component format ' + repr(magic))
    value = 2166136261
    for index, byte in enumerate(data):
        value = ((value ^ (0 if 12 <= index < 16 else byte)) * 16777619) & NO_HANDLE
    require(value == struct.unpack_from('<I', data, 12)[0], 'Component checksum mismatch ' + repr(magic))


def validate_saved(guest, payload, recipe, source_validation):
    state = guest['checkpoint_state']
    require(state[9] == 1 and state[3] == 0 and state[4] == len(payload), 'Ordinary source world save failed')
    sections = checkpoint_sections(payload)
    mover, clutter, trigger, vehicle = sections[3], sections[8], sections[5], sections[11]
    component(mover, b'RFMC', 1)
    component(clutter, b'RFPC', 1)
    component(trigger, b'RFTC', 1)
    require(len(mover) == 152 and struct.unpack_from('<I', mover, 16)[0] == 1, 'Expected one unchanged RFMC1 row')
    m = mover[64:]
    uid, kind, keys, flags, mode = struct.unpack_from('<5I', m)
    require((uid, kind, keys, mode) == (KEY, 2 if recipe['mode'] == 'yaw' else 1, 4, 1),
            'Saved controller identity/kind/keys/mode changed')
    phase, speed, distance = struct.unpack_from('<3f', m, 40)
    require(0 < phase < DURATION and all(math.isfinite(v) for v in (phase, speed, distance)),
            'Controller was not saved during motion')
    last = decode_rows(guest['extra'])[-1][0]
    close_vector([phase, distance], [f(last[21]), f(last[23])], .000001, 'saved actual controller phase')
    require(struct.unpack_from('<I', clutter, 16)[0] == 3 and len(clutter) == 136,
            'Expected unchanged RFPC1 three24-byte rows without poses')
    for index, uid in enumerate(CHILDREN[:3]):
        row = struct.unpack_from('<6I', clutter, 64 + 24 * index)
        require(row[0] == uid and row[2] == last[84 + index], 'Saved clutter UID or exact health changed')
        live = decode_rows(guest['extra'])[-1][1][index]
        require(row[3] == live[6] & 0x204002, 'Saved clutter gameplay flags changed')
    levels, count = struct.unpack_from('<2I', trigger, 16)
    require(levels == 1 and count == 1 and len(trigger) == 168, 'Expected one unchanged RFTC1 trigger')
    t = struct.unpack_from('<10I', trigger, 128)
    require(t[:3] == (0, TRIGGER, 1) and t[3] == (last[9] & ~64) and t[4] == 2 and
            t[5] == last[11] and t[7] == NO_HANDLE and t[3] & 16,
            'Saved disabled trigger/count/limit mismatch')
    require(vehicle[:4] == b'RFVA' and struct.unpack_from('<I', vehicle, 4)[0] == 2,
            'Expected unchanged passive RFVA2')
    inner, count = struct.unpack_from('<2I', vehicle, 8)
    require(count == 1 and len(vehicle) == 16 + inner + 80, 'RFVA2 owner row layout changed')
    v = vehicle[16 + inner:]
    require(struct.unpack_from('<2I', v) == (HOST, 1) and struct.unpack_from('<2f', v, 56) == (2000., 0.),
            'Saved Fighter exact binding/vitals changed')
    close_vector(list(struct.unpack_from('<3f', v, 8)), vector(last, 72), .000001, 'saved Fighter position')
    close_vector(list(struct.unpack_from('<9f', v, 20)), vector(last, 75, 9), .000001, 'saved Fighter all axes')
    return dict(formats=['RFWC%d' % struct.unpack_from('<I', payload, 4)[0], 'RFMC1', 'RFPC1', 'RFTC1', 'RFVA2'],
        payload_sha256=sha(payload), bytes=len(payload), count=t[4], flags=t[3], object_flags=t[5],
        controller_phase=phase, controller_distance=distance, controller_speed=speed,
        controller_pending=list(struct.unpack_from('<3f', m, 64)),
        controller_velocity=list(struct.unpack_from('<3f', m, 76)),
        handles=source_validation['handles'], clutter_health_bits=source_validation['clutter_health_bits'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--case', choices=('all', 'translation', 'yaw'), default='all')
    args = parser.parse_args()
    cases = ('translation', 'yaw') if args.case == 'all' else (args.case,)
    if args.prepare_only:
        for name in cases:
            target = args.prepare_only / name if len(cases) > 1 else args.prepare_only
            fixture, recipe = prepare_level(target, name)
            print(json.dumps(dict(status=recipe['status'], case=name, fixture=str(fixture),
                children=CHILDREN, source_sha256=recipe['source_sha256'], archive_sha256=recipe['archive_sha256']), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    require(base.is_file(), 'Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('vehicle-attached-controls-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {
        'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', 'campaign-attached-controls.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in names}
    report = dict(result='FAIL', scope=__doc__, cases={}, original_input_sha256={
        name: sha(data) if data is not None else None for name, data in original.items()})
    try:
        for name in cases:
            run = folder / name
            fixture, recipe = prepare_level(run / 'level', name)
            case = report['cases'][name] = dict(recipe=recipe)
            for flag in names:
                (DISC / flag).unlink(missing_ok=True)
            inputs = source_inputs(fixture, recipe)
            for flag, data in inputs.items():
                (DISC / flag).write_bytes(data)
            require(all((DISC / flag).read_bytes() == data for flag, data in inputs.items()), 'Source input staging mismatch')
            case['source_input_sha256'] = {flag: sha(data) for flag, data in inputs.items()}
            from xemu_world_hdd import prepare
            hdd = prepare(ROOT, base)
            case['isolated_hdd'] = str(hdd)
            build(run, 'source')
            case['source_binary_evidence'] = preserve_launch_binaries(run / 'source-binaries')
            guest = run_guest(run, 'source', hdd, FRAMES, 540, capture_world=True,
                extra_symbols=SYMBOLS, allow_guest_error=True)
            case['source'] = guest
            case['source_validation'] = validate_phase(guest, recipe)
            payload = (run / 'source/xbox-world.rfwc').read_bytes()
            case['saved'] = validate_saved(guest, payload, recipe, case['source_validation'])
            for flag in ('campaign-setup.bin', 'campaign-setup-immediate.flag', 'world-hdd-save.flag'):
                (DISC / flag).unlink(missing_ok=True)
            load_inputs = {'scene-preview.flag': b'', 'world-hdd-load.flag': b'1',
                'campaign-attached-controls.bin': U(0x54434652, 2, LOAD_FRAMES, 0),
                'player-replay.bin': replay(LOAD_FRAMES)}
            for flag, data in load_inputs.items():
                (DISC / flag).write_bytes(data)
            require(all((DISC / flag).read_bytes() == data for flag, data in load_inputs.items()), 'Load input staging mismatch')
            case['load_input_sha256'] = {flag: sha((DISC / flag).read_bytes()) for flag in names if (DISC / flag).is_file()}
            build(run, 'load')
            case['load_binary_evidence'] = preserve_launch_binaries(run / 'load-binaries')
            loaded = run_guest(run, 'load', hdd, LOAD_FRAMES, 480, snapshot=True,
                extra_symbols=SYMBOLS, allow_guest_error=True)
            case['load'] = loaded
            case['load_validation'] = validate_phase(loaded, recipe, loaded=True, saved=case['saved'])
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
        require(report['disc_restored'], 'Original disc inputs including scene-preview.flag were not restored')


if __name__ == '__main__':
    main()
