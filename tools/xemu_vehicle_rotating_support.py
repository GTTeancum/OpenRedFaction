"""Enemy-free, bounded Xbox passive-vehicle rotating support fixture.

Original L20S2 Fighter4717 and optional friendly/unarmed Eos4716 keep every
entity byte, including health. Hanger Lift001 is an explicitly edited fixture
controller: +Y axis through the Fighter center, 30 degrees over two seconds,
one-way. Original UnHide18359 fires at frame0; original When_Dead18354 starts
key4543 at frame60 through campaign-setup.bin. The watcher also links living
Fighter4717, preventing its automatic death poll from firing before frame60. Off-center roof placement and
support seeding at frame40 are explicit test setup, never natural landing.
All geometry stays byte-exact. Unrelated events, controllers, triggers and
navigation are removed; retained controller callbacks are cleared. No guest
memory writes, screenshots, host input, PC runtime or campaign-route claim.

--prepare-only independently checks disposable level metadata without building,
changing disc inputs, or running XEMU. Parent owns builds, runs and cleanup.
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
from xemu_vehicle_npc_push import (ACTOR, PUSHER, START_EVENT, SHOW_EVENT,
    LIFT_KEY, NO_HANDLE, actor_details, event_rows, section_payload)
from xemu_turret_combat import entity_rows
from xemu_native_world_save import ROOT, DISC, FLAGS, address, build, run_guest
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu

FRAMES, PLACE_FRAME, START_FRAME = 150, 40, 60
JUMP_FRAME, PROBE_FRAME, POST_FRAME, LOAD_FRAMES = 125, 110, 140, 20
DURATION, ROTATION_DEGREES = 2.0, 30.0
GROUP_NAME = 'Hanger Lift001'
AXIS = [0.0, 1.0, 0.0]
FIXTURE_MODES = {'player': 8, 'npc': 9}
LIMITATIONS = ('Explicit off-center support seeding on one passive attached Fighter; '
    'no natural landing, walking rider, crush, crowded contact, active vehicle or campaign claim.')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def f(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def xyz(row, at):
    return [f(v) for v in row[at:at + 3]]


def norm(point):
    return math.sqrt(sum(v * v for v in point))


def decode_group_spans(raw):
    """Independent field-span reader; offsets are within one complete group.

    The fixture writer uses inspect_moving_groups' records. This second reader
    walks raw count/string lengths rather than trusting those writer offsets.
    Layout follows core/level.c group_key/rf_level_group_next, v180.
    """
    at = 0

    def take(size):
        nonlocal at
        if size < 0 or size > len(raw) - at:
            raise ValueError('Moving-group field outside record')
        result = raw[at:at + size]
        at += size
        return result

    def uint():
        return struct.unpack('<I', take(4))[0]

    def string():
        return take(struct.unpack('<H', take(2))[0]).decode('cp1252')

    def floats(count):
        result = list(struct.unpack('<' + 'f' * count, take(count * 4)))
        if not all(math.isfinite(v) for v in result):
            raise ValueError('Nonfinite moving-group metadata')
        return result

    name, header, keys = string(), list(take(2)), []
    for _ in range(uint()):
        begin = at
        uid = uint()
        position_at = at
        position, disk = floats(3), floats(9)
        label, flag = string(), take(1)[0]
        timing_at = at
        timing = floats(5)
        links_at = at
        links = [uint() for _ in range(3)]
        rotation_at = at
        rotation = floats(1)[0]
        keys.append(dict(offset=begin, bytes=at - begin, uid=uid,
            position_at=position_at, position=position, orientation_disk=disk,
            runtime_basis=disk[3:] + disk[:3], label=label, flag=flag,
            timing_at=timing_at, timing=timing, links_at=links_at, links=links,
            rotation_at=rotation_at, rotation=rotation))
    legacy = []
    for _ in range(uint()):
        legacy.append(dict(uid=uint(), position=floats(3), orientation_disk=floats(9)))
    flags_at = at
    flags = list(take(6))
    mode_at = at
    mode, unknown = uint(), uint()
    sounds = [dict(name=string(), value=floats(1)[0]) for _ in range(4)]
    ids = [[uint() for _ in range(uint())] for _ in range(2)]
    if at != len(raw):
        raise ValueError('Moving-group record has trailing bytes')
    return dict(name=name, header=header, keys=keys, legacy=legacy,
                flags_at=flags_at, flags=flags, mode_at=mode_at, mode=mode,
                unknown=unknown, sounds=sounds, ids1=ids[0], ids2=ids[1])


def replace_links(row, links):
    return row['raw'][:row['links_at']] + U(len(links), *links) + row['raw'][row['links_end']:]


def rotating_group(group_data, group, center):
    raw = group_data[group['offset']:group['offset'] + group['bytes']]
    independent = decode_group_spans(raw)
    for index, key in enumerate(group['keys']):
        checked = independent['keys'][index]
        if checked['offset'] != key['offset'] - group['offset'] or \
                any(checked[name] != key[name] for name in
                    ('bytes', 'uid', 'position', 'orientation_disk', 'timing', 'links', 'rotation')):
            raise ValueError('Independent moving-group decoder disagrees with inventory')
    if independent['flags'] != [1, 0, 0, 0, 0, 0] or independent['mode'] != 2 or \
            independent['unknown'] != 0 or independent['keys'][0]['runtime_basis'] != [1.,0.,0.,0.,1.,0.,0.,0.,1.]:
        raise ValueError('Original Hanger Lift001 controller metadata changed')
    out = bytearray(raw)
    changes = []

    def patch(label, offset, data):
        before = raw[offset:offset + len(data)]
        if len(before) != len(data):
            raise ValueError('Patch exceeds moving-group record')
        out[offset:offset + len(data)] = data
        changes.append(dict(field=label, offset=offset, bytes=len(data),
                            before_hex=before.hex(), after_hex=data.hex()))

    first = independent['keys'][0]
    patch('key4543.position = original Fighter4717 center', first['position_at'],
          struct.pack('<3f', *center))
    patch('key4543.timing = wait,forward,reverse,acceleration,deceleration',
          first['timing_at'], struct.pack('<5f', 0, DURATION, DURATION, 0, 0))
    patch('key4543.rotation_degrees', first['rotation_at'], struct.pack('<f', ROTATION_DEGREES))
    patch('group.flags[1] = rotation', independent['flags_at'] + 1, b'\x01')
    patch('group.mode = one-way', independent['mode_at'], U(1))
    for key in independent['keys']:
        patch('key%d.callbacks = none' % key['uid'], key['links_at'], U(NO_HANDLE, NO_HANDLE, NO_HANDLE))
    permitted = set()
    for change in changes:
        permitted.update(range(change['offset'], change['offset'] + change['bytes']))
    if any(a != b and i not in permitted for i, (a, b) in enumerate(zip(raw, out))):
        raise ValueError('Undocumented moving-group edit')
    checked = decode_group_spans(out)
    if checked['keys'][0]['position'] != center or checked['keys'][0]['runtime_basis'][3:6] != AXIS or \
            checked['keys'][0]['timing'] != [0., DURATION, DURATION, 0., 0.] or \
            checked['keys'][0]['rotation'] != ROTATION_DEGREES or checked['flags'] != [1,1,0,0,0,0] or \
            checked['mode'] != 1 or any(k['links'] != [NO_HANDLE] * 3 for k in checked['keys']):
        raise ValueError('Explicit yaw fixture fields failed raw round trip')
    for field in ('name', 'header', 'legacy', 'unknown', 'sounds', 'ids1', 'ids2'):
        if checked[field] != independent[field]:
            raise ValueError('Fixture changed unrelated group field ' + field)
    # level.c: flags[2]==0 -> forward0x2000; flags[0] ->2; flags[1]->4.
    # There is no ramp because timing[3:5] are zero. Mover rotation-sign stays
    # +1 because initial flags include0x2000, even with the original ids2 mover.
    initial_flags = 0x80002006
    return bytes(out), dict(edits=changes, initial_flags=initial_flags,
        initial_flags_hex=hex(initial_flags), runtime_kind='RF_GROUP_RUNTIME_ROTATION_PENDING',
        rotation_sign=1, decoded=checked)


def prepare_level(folder, rider='npc'):
    if rider not in FIXTURE_MODES:
        raise ValueError('Unknown rider ' + rider)
    source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L20S2.rfl'))
    if meta['version'] != 180 or meta['trailing_bytes']:
        raise ValueError('Expected complete original v180 L20S2')
    entities = entity_rows(source)
    actor = next(r for r in entities if r['uid'] == ACTOR)
    host = next(r for r in entities if r['uid'] == PUSHER)
    details, host_details = actor_details(actor), actor_details(host)
    if actor['name'] != 'eos' or host['name'] != 'masako_fighter' or details != dict(
            relationship=1, friendliness=2, health=1., armor=0., primary='none',
            secondary='none', seat_host_uid=-1, creation_flags=4) or \
            host_details['health'] != 2000. or host_details['armor'] != 0.:
        raise ValueError('Original authored rider/chassis fields changed')
    center = list(struct.unpack_from('<3f', host['raw'], host['transform']))
    disk_basis = list(struct.unpack_from('<9f', host['raw'], host['transform'] + 12))
    initial_basis = disk_basis[3:] + disk_basis[:3]
    retained = [host['raw']] + ([actor['raw']] if rider == 'npc' else [])
    expected_uids = [PUSHER] + ([ACTOR] if rider == 'npc' else [])
    events = event_rows(section_payload(source, meta, 0x600))
    start = next(r for r in events if r['uid'] == START_EVENT)
    show = next(r for r in events if r['uid'] == SHOW_EVENT)
    if start['type'] != 'When_Dead' or LIFT_KEY not in start['links'] or \
            show['type'] != 'UnHide' or PUSHER not in show['links']:
        raise ValueError('Original lift start/reveal outputs changed')
    # runtime_death_poll excludes controllers from living inputs. A watcher
    # linked only to key4543 would therefore auto-fire at startup. Retain the
    # real healthy chassis as its living gate; startup_target ignores the non-event/controller
    # vehicle targets (counts other_targets, leaves status unchanged), then
    # activates kind8 key4543 on the explicit setup output at frame60.
    start_links = [PUSHER, LIFT_KEY]
    flags_at = 4 + 2 + len(start['type']) + 12 + 2 + len(start['name']) + 1 + 4
    if list(start['raw'][flags_at:flags_at + 2]) != [0, 0]:
        raise ValueError('Original When_Dead polling flags changed')
    setup, reveal = replace_links(start, start_links), replace_links(show, [PUSHER])
    group_data = section_payload(source, meta, 0x3000)
    groups = inspect_groups(group_data)
    group = next(g for g in groups if g['name'] == GROUP_NAME)
    if group['keys'][0]['uid'] != LIFT_KEY or PUSHER not in group['ids1']:
        raise ValueError('Original Fighter/controller ownership changed')
    retained_group, controller = rotating_group(group_data, group, center)
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
    actual = entity_rows(out)
    if [r['uid'] for r in actual] != expected_uids or [r['raw'] for r in actual] != retained:
        raise ValueError('Fixture did not retain byte-exact authored entities')
    checked_events = event_rows(section_payload(out, checked, 0x600))
    if [r['raw'] for r in checked_events] != [setup, reveal] or \
            [r['links'] for r in checked_events] != [start_links, [PUSHER]]:
        raise ValueError('Fixture retained an unrelated event or changed event data')
    checked_groups = inspect_groups(section_payload(out, checked, 0x3000))
    if len(checked_groups) != 1 or checked_groups[0]['ids1'] != group['ids1'] or \
            checked_groups[0]['ids2'] != group['ids2']:
        raise ValueError('Fixture altered original group membership')
    section_checks = []
    for section in meta['sections']:
        kind = int(section['type'], 16)
        if kind not in replacements:
            a, b = section_payload(source, meta, kind), section_payload(out, checked, kind)
            if a != b:
                raise ValueError('Unexpected original section edit: %x' % kind)
            section_checks.append(dict(type=hex(kind), bytes=len(a), sha256=sha(a)))
    folder.mkdir(parents=True, exist_ok=True)
    fixture = folder / 'scene-fixture.vpp'
    if fixture.exists() and fixture.stat().st_nlink != 1:
        raise ValueError('Refusing to replace a linked fixture archive')
    archive(fixture, [('L20S2.rfl', out)])
    if read_entry(fixture, 'L20S2.rfl') != out:
        raise ValueError('Disposable VPP round trip failed')
    recipe = dict(status='PREPARED_NOT_RUN', source='levels2.vpp/L20S2.rfl',
        source_sha256=sha(source), fixture_sha256=sha(out), archive_sha256=sha(fixture.read_bytes()),
        rider=rider, actor_uid=ACTOR if rider == 'npc' else None, host_uid=PUSHER,
        entity_uids=expected_uids, removed_entities=len(entities) - len(retained),
        actor_details=details if rider == 'npc' else None, host_details=host_details,
        source_actor_sha256=sha(actor['raw']) if rider == 'npc' else None,
        source_host_sha256=sha(host['raw']), controller=controller,
        center=center, initial_basis=initial_basis, axis=AXIS, rotation_degrees=ROTATION_DEGREES,
        duration_seconds=DURATION, radians_per_tick=-math.radians(ROTATION_DEGREES) / (DURATION * 60),
        first_output=dict(uid=SHOW_EVENT, frame=0, links=[PUSHER]),
        second_output=dict(uid=START_EVENT, frame=START_FRAME, links=start_links),
        startup_death_gate=dict(living_input_uid=PUSHER, health=host_details['health'],
            watcher_flags=[0, 0],
            evidence='core/event.c runtime_death_poll: living input suppresses automatic fire; startup_target ignores non-event/controller vehicle input and activates kind8 controller output; scene.c campaign_death_query resolves exact passive owner health.'),
        original_start_links=start['links'], original_reveal_links=show['links'],
        removed_events=len(events) - 2, removed_groups=len(groups) - 1,
        original_triggers=struct.unpack_from('<I', section_payload(source, meta, 0x60000))[0],
        frames=FRAMES, placement_frame=PLACE_FRAME, fixture_mode=FIXTURE_MODES[rider],
        preserved_sections=section_checks,
        matrix_rule='For row-vector basis: local[i]=dot(p-old_center,old_basis_row[i]); target[j]=new_center[j]+sum(local[i]*new_basis_row[i][j]). +Y yaw: x=c*x+s*z; z=-s*x+c*z.',
        roof_placement='Explicit sphere-derived roof point with nonzero X/Z radius about chassis center; parent stimulus owns exact safe point and support seeding.',
        timing_note='Normal controller remains inactive until original When_Dead output at frame60; frame40 support seed occurs during stationary phase. Source150 frames leaves the two-second rotation in progress.',
        limitations=LIMITATIONS)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    return fixture, recipe



def point_target(point, previous_center, previous_basis, center, basis):
    """Independent row-basis rigid transform used by runtime validation."""
    if any(len(v) != size for v, size in ((point, 3), (previous_center, 3),
            (previous_basis, 9), (center, 3), (basis, 9))) or not all(
                math.isfinite(v) for values in (point, previous_center, previous_basis, center, basis)
                for v in values):
        raise ValueError('Invalid support transform input')
    local = [sum((point[j] - previous_center[j]) * previous_basis[i * 3 + j]
                 for j in range(3)) for i in range(3)]
    return [center[j] + sum(local[i] * basis[i * 3 + j] for i in range(3))
            for j in range(3)]


def accepted_velocity(point, previous_center, previous_basis, center, basis):
    previous = point_target(point, center, basis, previous_center, previous_basis)
    return [(point[i] - previous[i]) * 60 for i in range(3)]


def preserve_launch_binaries(folder):
    """Call after the fixture build and immediately before launching its guest.

    Preserve the actual prelaunch executable/map, rather than a later restore
    build's files. Parent cleanup may prune payload copies after compact hash
    evidence has been retained in report.json.
    """
    evidence = {}
    folder.mkdir(parents=True, exist_ok=True)
    for label, path in (('default.xbe', DISC / 'default.xbe'),
                        ('main.exe', ROOT / 'build/xbox/main.exe'),
                        ('main.map', ROOT / 'build/xbox/main.map')):
        data = path.read_bytes()
        target = folder / label
        target.write_bytes(data)
        if target.read_bytes() != data:
            raise RuntimeError('Prelaunch binary evidence copy failed: ' + label)
        evidence[label] = dict(sha256=sha(data), bytes=len(data), copy=str(target))
    return evidence


def source_inputs(fixture, recipe):
    """Return explicit source input bytes; no disc side effects."""
    return {'scene-fixture.vpp': fixture.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'),
        'scene-preview.flag': b'', 'player-control.flag': b'', 'campaign-spawn.flag': b'',
        'campaign-actor.bin': U(PUSHER),
        'campaign-setup.bin': U(SHOW_EVENT, START_EVENT),
        'campaign-passive-roof.bin': U(PUSHER, PLACE_FRAME, recipe['fixture_mode']),
        'player-replay.bin': replay(FRAMES, jump=recipe['rider'] == 'player')}


SYMBOLS = {
    'rf_scene_passive_support_motion': 96, 'rf_scene_passive_support_live': 32,
    'rf_scene_passive_support_release': 8, 'rf_scene_passive_roof_fixture': 10,
    'rf_scene_passive_npc_fixture': 10, 'rf_scene_passive_attachment': 14,
    'rf_scene_passive_damage': 8, 'rf_scene_passive_draw': 6,
    'rf_scene_vehicle_visibility': 8, 'rf_scene_enemy_combat': 8,
    'rf_scene_setup_result': 4, 'rf_scene_rotating_doors': 8,
    'rf_scene_player_jump': 4, 'rf_scene_npc_support_lifecycle': 8,
    'rf_scene_npc_checkpoint_reject_state': 6, 'rf_scene_world_load_reject': 3,
    'rf_scene_checkpoint_world_reject': 9,
}


def replay(frames, jump=False):
    return b'RFI6' + U(48) + b''.join(struct.pack('<5f7I',
        0, 0, 0, 0, 0, 0, int(jump and frame == JUMP_FRAME), 0, 0, 0, 0, 0)
        for frame in range(frames))


def live_probe(monitor, mapping):
    result = {name: words(monitor, address(mapping, name), count)
              for name, count in SYMBOLS.items()}
    result['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return result


def close_vector(a, b, tolerance, label):
    if len(a) != len(b) or not all(math.isfinite(v) for v in a + b) or \
            any(abs(x - y) > tolerance for x, y in zip(a, b)):
        raise RuntimeError('%s: %r != %r (tolerance %g)' % (label, a, b, tolerance))


def motion_row(raw, accepted, actor_handle, host_handle, recipe):
    if len(raw) != 48 or not raw[0] or raw[1:4] != [actor_handle, host_handle, PUSHER] or \
            not raw[4] & 2 or raw[5] != 1 or raw[7]:
        raise RuntimeError('Missing fresh full-owner rotational sample: %r' % raw)
    point, target, velocity = xyz(raw, 8), xyz(raw, 11), xyz(raw, 14)
    start, end = xyz(raw, 17), xyz(raw, 20)
    old_basis, basis = [f(v) for v in raw[23:32]], [f(v) for v in raw[32:41]]
    close_vector(start, recipe['center'], .0002, 'old chassis center')
    close_vector(end, recipe['center'], .0002, 'new chassis center')
    if f(raw[41]) != 2000. or raw[42] & 0x4000:
        raise RuntimeError('Rotating source is hidden or its authored health changed')
    expected_target = point_target(point, end, basis, start, old_basis) if accepted else \
        point_target(point, start, old_basis, end, basis)
    expected_velocity = accepted_velocity(point, start, old_basis, end, basis) if accepted else \
        [(expected_target[i] - point[i]) * 60 for i in range(3)]
    close_vector(target, point if accepted else expected_target, .001,
                 'accepted unchanged end point' if accepted else 'forward proposal')
    close_vector(velocity, expected_velocity, .02, 'rigid point velocity')
    # Independent axis-angle rule, not merely an implementation-to-itself check.
    angle = recipe['radians_per_tick']
    c, s = math.cos(angle), math.sin(angle)
    yaw_basis = []
    for at in (0, 3, 6):
        x, y, z = old_basis[at:at + 3]
        yaw_basis.extend([c * x + s * z, y, -s * x + c * z])
    close_vector(basis, yaw_basis, .0001, 'normal controller +Y yaw step')
    if norm(velocity) < .05:
        raise RuntimeError('Off-center rotation lacks measurable point velocity')
    return dict(calls=raw[0], frame=raw[6], actor_handle=actor_handle, host_handle=host_handle,
        input=point, target=target, velocity=velocity, center=end, old_basis=old_basis, basis=basis)


def checked_live(sample, recipe, require_support=True):
    row_index = 16 if recipe['rider'] == 'npc' else 0
    live = sample['rf_scene_passive_support_live'][row_index:row_index + 16]
    attachment = sample['rf_scene_passive_attachment']
    if attachment[:2] != [1, 1] or attachment[3] or attachment[4] != PUSHER or attachment[13]:
        raise RuntimeError('Original attached owner/controller was lost: %r' % attachment)
    host = attachment[5]
    actor = live[2]
    if not live[0] or live[15] or not actor or actor in (host, NO_HANDLE) or not host or host == NO_HANDLE:
        raise RuntimeError('Missing actor or full owner handle in live support row: %r' % live)
    if live[13] != NO_HANDLE or f(live[5]) <= 0:
        raise RuntimeError('Rider became seated or died: %r' % live)
    if recipe['rider'] == 'npc' and (f(live[5]) != 1. or live[12] & (2 | 8 | 0x4000)):
        raise RuntimeError('Fixture NPC lost its original living eligible state')
    if require_support and (live[3] != host or live[4] == 3):
        raise RuntimeError('Actor is not supported by its exact chassis owner: %r' % live)
    if not require_support and (live[3] != 0 or live[4] != 3 or live[14] & 0x400000):
        raise RuntimeError('Jump did not release the exact support handle: %r' % live)
    return live, actor, host


def validate_supported_sample(sample, recipe):
    live, actor, host = checked_live(sample, recipe)
    raw = sample['rf_scene_passive_support_motion']
    forward = motion_row(raw[:48], False, actor, host, recipe)
    accepted = motion_row(raw[48:], True, actor, host, recipe)
    if live[1] != accepted['frame'] or forward['frame'] != accepted['frame']:
        raise RuntimeError('Forward/accepted/live samples are not from the same interval')
    close_vector([xyz(live, 6)[i] for i in (0, 2)],
                 [forward['target'][i] for i in (0, 2)], .002, 'accepted actual forward carry X/Z')
    if live[9:12] != raw[62:65]:
        raise RuntimeError('Live support cache is not the exact accepted-point velocity')
    close_vector(xyz(live, 6), accepted['input'], .008, 'live accepted actor position')
    close_vector(xyz(live, 9), accepted['velocity'], .02, 'live accepted support cache')
    return dict(frame=live[1], actor_handle=actor, host_handle=host,
                position=xyz(live, 6), velocity=xyz(live, 9), forward=forward, accepted=accepted)


def validate_source(guest, recipe):
    if guest['guest_phase'] != 5 or guest['frames'] != FRAMES or \
            guest['memory_bytes'] != 64 * 1024 * 1024 or guest['free_pages'] <= 0 or guest['player_life'][2]:
        raise RuntimeError('Incomplete living stock64MiB bounded source')
    pre, post, end = guest['probe'], guest['final_probe'], guest['extra']
    if not PROBE_FRAME <= pre['frame'] < JUMP_FRAME or not POST_FRAME <= post['frame'] < FRAMES:
        raise RuntimeError('Live probe missed its bounded window')
    for sample in (pre, post, end):
        damage, combat = sample['rf_scene_passive_damage'], sample['rf_scene_enemy_combat']
        if damage[2] or damage[3] or damage[6] or damage[7] != 1 or combat[2] or combat[3] or combat[7]:
            raise RuntimeError('Enemy-free source unexpectedly caused combat or vehicle damage')
        if sample['rf_scene_setup_result'] != [2, START_EVENT, 16, 0]:
            raise RuntimeError('Expected only original reveal0/start60 setup activations')
        rotating = sample['rf_scene_rotating_doors']
        if not rotating[0] or rotating[1] or rotating[2] != LIFT_KEY or rotating[7]:
            raise RuntimeError('Normal one-way controller did not remain mid-rotation')
    end_rotation = end['rf_scene_rotating_doors']
    if abs(end_rotation[0] - (FRAMES - START_FRAME)) > 1 or \
            abs(f(end_rotation[3]) - end_rotation[0] * recipe['radians_per_tick']) > .0001:
        raise RuntimeError('Controller timing/angle does not match the stationary60-frame prephase')
    fixture = end['rf_scene_passive_roof_fixture']
    if fixture[:3] != [PUSHER, PLACE_FRAME, 1] or fixture[9] != recipe['fixture_mode']:
        raise RuntimeError('Expected one explicit off-center support seed: %r' % fixture)
    initial = xyz(fixture, 3)
    if math.hypot(initial[0] - recipe['center'][0], initial[2] - recipe['center'][2]) < .49:
        raise RuntimeError('Roof seed is not meaningfully off-center')
    first = validate_supported_sample(pre, recipe)
    expected = point_target(initial, recipe['center'], recipe['initial_basis'],
                            first['accepted']['center'], first['accepted']['basis'])
    close_vector([first['position'][i] for i in (0, 2)], [expected[i] for i in (0, 2)],
                 .025, 'live off-center angular carry from stationary seed')
    seed_radius = math.hypot(initial[0] - recipe['center'][0], initial[2] - recipe['center'][2])
    live_radius = math.hypot(first['position'][0] - recipe['center'][0],
                             first['position'][2] - recipe['center'][2])
    if abs(live_radius - seed_radius) > .025:
        raise RuntimeError('Cumulative carry changed off-axis roof radius')
    if norm([first['position'][i] - initial[i] for i in (0, 2)]) < .07:
        raise RuntimeError('Off-center actor did not move with the real rotation')
    result = dict(result='PASS', rider=recipe['rider'], initial_position=initial,
                  pre=first, expected_pre_position=expected, initial_radius=seed_radius,
                  pre_radius=live_radius, free_pages=guest['free_pages'], limitations=LIMITATIONS)
    if recipe['rider'] == 'npc':
        for label, sample in (('post', post), ('end', end)):
            result[label] = validate_supported_sample(sample, recipe)
            expected_later = point_target(initial, recipe['center'], recipe['initial_basis'],
                result[label]['accepted']['center'], result[label]['accepted']['basis'])
            close_vector([result[label]['position'][i] for i in (0, 2)],
                [expected_later[i] for i in (0, 2)], .025, 'continued NPC global angular carry')
            npc = sample['rf_scene_passive_npc_fixture']
            if npc[0] != first['actor_handle'] or npc[1] != first['host_handle'] or npc[5] or f(npc[6]) != 1.:
                raise RuntimeError('Idle authored NPC changed identity, script or health')
        if end['rf_scene_npc_support_lifecycle'][0] != pre['rf_scene_npc_support_lifecycle'][0] or \
                end['rf_scene_npc_support_lifecycle'][7]:
            raise RuntimeError('NPC lost support unexpectedly during gentle yaw')
    else:
        release = end['rf_scene_passive_support_release']
        if release[:4] != [1, JUMP_FRAME, first['host_handle'], first['actor_handle']] or release[7]:
            raise RuntimeError('Expected exactly one normal jump release: %r' % release)
        jump = end['rf_scene_player_jump']
        if jump[0] != 1 or jump[1] != 1 or jump[3] != JUMP_FRAME:
            raise RuntimeError('Ordinary replay jump did not execute exactly once')
        raw_motion = end['rf_scene_passive_support_motion']
        last = motion_row(raw_motion[48:], True, first['actor_handle'], first['host_handle'], recipe)
        if not START_FRAME <= last['frame'] <= JUMP_FRAME:
            raise RuntimeError('Accepted support cache changed after jump release')
        close_vector(xyz(release, 4), last['velocity'], .02, 'release retained last accepted point velocity')
        for label, sample in (('post', post), ('end', end)):
            live, actor, host = checked_live(sample, recipe, False)
            if actor != first['actor_handle'] or host != first['host_handle'] or live[9:12] != release[4:7]:
                raise RuntimeError('Airborne support cache changed or owner identity aliased')
            if sample['rf_scene_passive_support_release'] != release:
                raise RuntimeError('Player release replayed after the original jump')
            result[label] = dict(frame=live[1], position=xyz(live, 6), retained_velocity=xyz(live, 9))
        if post['rf_scene_rotating_doors'][0] <= pre['rf_scene_rotating_doors'][0]:
            raise RuntimeError('Chassis stopped when the rider released it')
        result['release'] = dict(frame=release[1], velocity=xyz(release, 4), last_accepted=last)
    return result


def checkpoint_sections(payload):
    if payload[:4] != b'RFWC' or len(payload) < 320:
        raise RuntimeError('Missing ordinary RFWC world payload')
    version, size, checksum, count = struct.unpack_from('<4I', payload, 4)
    if version not in (2, 3) or count != (16 if version == 2 else 17) or size != len(payload):
        raise RuntimeError('Unexpected ordinary world envelope')
    h = 2166136261
    for at, byte in enumerate(payload):
        h = ((h ^ (0 if 12 <= at < 16 else byte)) * 16777619) & NO_HANDLE
    if h != checksum:
        raise RuntimeError('Ordinary world checksum mismatch')
    result, cursor = {}, 128 + count * 12
    for index in range(count):
        kind, offset, length = struct.unpack_from('<3I', payload, 128 + index * 12)
        if kind != index + 1 or offset != cursor or length > len(payload) - cursor:
            raise RuntimeError('World directory is not complete/canonical')
        result[kind] = payload[offset:offset + length]
        cursor += length
    if cursor != len(payload):
        raise RuntimeError('Trailing ordinary world payload')
    return result


def validate_saved(guest, payload, recipe):
    state = guest['checkpoint_state']
    if state[9] != 1 or state[3] or state[4] != len(payload):
        raise RuntimeError('Existing ordinary NPC save codec did not admit the fixture: %r' % state)
    sections = checkpoint_sections(payload)
    npc, vehicle, mover = sections[2], sections[11], sections[3]
    if npc[:4] != b'RFNC' or struct.unpack_from('<I', npc, 4)[0] != 10 or \
            struct.unpack_from('<I', npc, 16)[0] != 1 or len(npc) < 664:
        raise RuntimeError('Expected one ordinary unchanged-format RFNC10 NPC')
    n = npc[64:]
    if struct.unpack_from('<I', n)[0] != ACTOR or struct.unpack_from('<2f', n, 20) != (1., 0.) or \
            struct.unpack_from('<I', n, 572)[0] != PUSHER:
        raise RuntimeError('RFNC lost exact authored rider, original vitals or support UID')
    span = 600 + sum(struct.unpack_from('<I', n, at)[0] for at in (540, 564, 568)) + \
        24 * struct.unpack_from('<I', n, 588)[0]
    if span != len(n):
        raise RuntimeError('RFNC row extensions do not exhaust the one saved actor')
    if vehicle[:4] != b'RFVA' or struct.unpack_from('<I', vehicle, 4)[0] != 2:
        raise RuntimeError('Expected unchanged RFVA2 passive-owner component')
    host_bytes, count = struct.unpack_from('<2I', vehicle, 8)
    if count != 1 or len(vehicle) != 16 + host_bytes + 80:
        raise RuntimeError('RFVA2 did not preserve exactly one passive chassis')
    v = vehicle[16 + host_bytes:]
    if struct.unpack_from('<2I', v) != (PUSHER, 1) or struct.unpack_from('<2f', v, 56) != (2000., 0.):
        raise RuntimeError('RFVA2 changed authored owner, binding or vitals')
    if mover[:4] != b'RFMC' or struct.unpack_from('<I', mover, 4)[0] != 1 or len(mover) != 152 or \
            struct.unpack_from('<I', mover, 16)[0] != 1 or struct.unpack_from('<I', mover, 64)[0] != LIFT_KEY:
        raise RuntimeError('Expected one unchanged-format RFMC1 controller')
    m = mover[64:]
    kind, key_count, flags, mode = struct.unpack_from('<4I', m, 4)
    phase, speed, angle = struct.unpack_from('<3f', m, 40)
    if kind != 2 or key_count != 4 or not flags & 4 or not flags & 0x2000 or mode != 1 or \
            not 0 < phase < DURATION or not all(math.isfinite(x) for x in (phase, speed, angle)) or \
            abs(angle - f(guest['extra']['rf_scene_rotating_doors'][3])) > .0001:
        raise RuntimeError('RFMC did not retain the actual in-progress one-way yaw state')
    live, _, _ = checked_live(guest['extra'], recipe)
    position, velocity = list(struct.unpack_from('<3f', n, 28)), list(struct.unpack_from('<3f', n, 576))
    center, basis = list(struct.unpack_from('<3f', v, 8)), list(struct.unpack_from('<9f', v, 20))
    close_vector(position, xyz(live, 6), .001, 'saved NPC exact live position')
    close_vector(velocity, xyz(live, 9), .0001, 'saved NPC exact support velocity')
    close_vector(center, recipe['center'], .0002, 'saved fixed chassis center')
    motion = guest['extra']['rf_scene_passive_support_motion'][48:]
    close_vector(basis, [f(v) for v in motion[32:41]], .00001, 'saved exact chassis basis')
    return dict(position=position, velocity=velocity, center=center, basis=basis,
                support_uid=PUSHER, npc_uid=ACTOR, formats=['RFNC10', 'RFVA2', 'RFMC1'],
                controller_angle=angle, controller_phase=phase)


def validate_loaded(guest, saved, recipe):
    if guest['guest_phase'] != 5 or guest['frames'] != LOAD_FRAMES or \
            guest['memory_bytes'] != 64 * 1024 * 1024 or guest['free_pages'] <= 0 or guest['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB fresh-load continuation')
    state = guest['checkpoint_state']
    if state[8] != 1 or state[0]:
        raise RuntimeError('Existing ordinary codec rejected fresh load: %r' % state)
    result = {}; handles = None
    for label, sample in (('pre', guest['probe']), ('post', guest['final_probe']), ('end', guest['extra'])):
        if any(sample['rf_scene_setup_result']) or any(sample['rf_scene_passive_roof_fixture']):
            raise RuntimeError('Fresh load replayed setup or fixture support seeding')
        result[label] = validate_supported_sample(sample, recipe)
        actual = result[label]
        current_handles = (actual['actor_handle'], actual['host_handle'])
        if handles is not None and current_handles != handles:
            raise RuntimeError('Fresh continuation changed the rider or host full handle')
        handles = current_handles
        rotation = sample['rf_scene_rotating_doors']
        expected_angle = saved['controller_angle'] + rotation[0] * recipe['radians_per_tick']
        if not 0 < rotation[0] < LOAD_FRAMES or rotation[1] or rotation[2] != LIFT_KEY or rotation[7] or \
                not math.isfinite(f(rotation[3])) or abs(f(rotation[3]) - expected_angle) > .00001:
            raise RuntimeError('Fresh continuation reset or replayed the saved controller angle')
        expected = point_target(saved['position'], saved['center'], saved['basis'],
                                actual['accepted']['center'], actual['accepted']['basis'])
        close_vector([actual['position'][i] for i in (0, 2)],
                     [expected[i] for i in (0, 2)], .04, 'fresh-load rotation without snapback')
    if norm([result['end']['position'][i] - saved['position'][i] for i in (0, 2)]) < .025:
        raise RuntimeError('Fresh-loaded rider did not continue its real angular travel')
    result.update(result='PASS', free_pages=guest['free_pages'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--case', choices=('all', 'player', 'npc'), default='all')
    parser.add_argument('--rider', choices=tuple(FIXTURE_MODES), help=argparse.SUPPRESS)
    parser.add_argument('--npc-save', action='store_true',
                        help='Also attempt existing ordinary NPC save/fresh-load codecs, without format changes')
    args = parser.parse_args()
    selected = args.rider or args.case
    cases = ('player', 'npc') if selected == 'all' else (selected,)
    if args.prepare_only:
        for name in cases:
            folder = args.prepare_only / name if len(cases) > 1 else args.prepare_only
            fixture, recipe = prepare_level(folder, name)
            print(json.dumps(dict(result='PASS', status=recipe['status'], fixture=str(fixture),
                entity_uids=recipe['entity_uids'], axis=recipe['axis'], center=recipe['center'],
                rotation_degrees=recipe['rotation_degrees'], duration_seconds=recipe['duration_seconds']), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('vehicle-rotating-support-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in names}
    report = dict(result='FAIL', scope=__doc__, cases={}, original_input_sha256={
        name: sha(data) if data is not None else None for name, data in original.items()})
    try:
        for name in cases:
            case_folder = folder / name
            fixture, recipe = prepare_level(case_folder / 'level', name)
            case = dict(recipe=recipe)
            report['cases'][name] = case
            for flag in names:
                (DISC / flag).unlink(missing_ok=True)
            for flag, data in source_inputs(fixture, recipe).items():
                (DISC / flag).write_bytes(data)
            save = name == 'npc' and args.npc_save
            hdd = base
            if save:
                from xemu_world_hdd import prepare
                hdd = prepare(ROOT, base)
                (DISC / 'world-hdd-save.flag').write_bytes(b'1')
            build(case_folder, 'source')
            case['source_binary_evidence'] = preserve_launch_binaries(case_folder / 'source-binaries')
            guest = run_guest(case_folder, 'source', hdd, FRAMES, 480, snapshot=not save,
                capture_world=save, extra_symbols=SYMBOLS, probe=live_probe, probe_frame=PROBE_FRAME,
                final_probe=live_probe, final_probe_frame=POST_FRAME, allow_guest_error=True)
            case['source'] = guest
            case['validation'] = validate_source(guest, recipe)
            if save:
                payload = (case_folder / 'source/xbox-world.rfwc').read_bytes()
                case['saved'] = validate_saved(guest, payload, recipe)
                for flag in ('campaign-setup.bin', 'campaign-passive-roof.bin', 'world-hdd-save.flag'):
                    (DISC / flag).unlink(missing_ok=True)
                (DISC / 'world-hdd-load.flag').write_bytes(b'1')
                (DISC / 'player-replay.bin').write_bytes(replay(LOAD_FRAMES))
                build(case_folder, 'load')
                case['load_binary_evidence'] = preserve_launch_binaries(case_folder / 'load-binaries')
                loaded = run_guest(case_folder, 'load', hdd, LOAD_FRAMES, 420, snapshot=True,
                    extra_symbols=SYMBOLS, probe=live_probe, probe_frame=5,
                    final_probe=live_probe, final_probe_frame=15, allow_guest_error=True)
                case['load'] = loaded
                case['load_validation'] = validate_loaded(loaded, case['saved'], recipe)
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
            raise RuntimeError('Original scene-preview flag or staged disc inputs were not restored')


if __name__ == '__main__':
    main()
