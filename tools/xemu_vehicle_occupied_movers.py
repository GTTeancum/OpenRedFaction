"""Bounded stock64MiB Xbox world movers while an independent Jeep is occupied.

Reuse the successful L20S2 attached-controls translation fixture: original
Fighter4717, lamps4527/4528, switch4529, Use trigger4530 and controller4543.
All L20S2 geometry and these owner poses remain byte-exact. Original friendly,
unarmed Eos4716 is retained and explicitly seeded on the Fighter at40 by the
existing roof mode9. One ordinary current-center trigger Use starts at45.
Original Jeep7629 is placed on independently decoded hangar floor78; only its
transform and the player start are changed. Real RFI6 Use90/120/150 boards,
exits and boards again. Save180 remains occupied. Freshload80 does no reveal,
trigger-start or rider seeding; Use30/60 exits and reboards the restored Jeep.

Read-only snapshots check exactly one controller advance/commit per eligible
world step, coherent child/Fighter/NPC motion, and stable full owner handles
across both state transitions and the ordinary save/fresh-process load. This
does not prove natural trigger approach or rider landing, collision/crush,
other mover types, audiovisual behavior or campaign progression.

--prepare-only writes disposable fixture/recipe, with no build or disc edits.
Parent owns serial builds, emulator sessions, runtime staging and cleanup.
No images, host input automation, guest writes, PC runtime or user PC.
"""

import argparse
import datetime
import io
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from inspect_geometry import inspect as inspect_geometry
from inspect_levels import inspect
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_turret_combat import entity_rows
from xemu_vehicle_attached_controls import (HOST, KEY, TRIGGER, SHOW, CHILDREN,
    NO_HANDLE, DURATION, prepare_level as prepare_controls, component,
    sha, f, vector, norm, require, expected_pose)
from xemu_vehicle_mixed_types import model_geometry, metadata
from xemu_vehicle_npc_push import actor_details, section_payload
from xemu_vehicle_rotating_support import close_vector, checkpoint_sections, preserve_launch_binaries

JEEP, RIDER = 7629, 4716
FRAMES, LOAD_FRAMES, START_FRAME = 180, 80, 45
SOURCE_FRAMES = [40, 45, 89, 90, 119, 120, 149, 150, 178]
LOAD_SAMPLES = [1, 2, 29, 30, 31, 59, 60, 61, 78]
DISK_IDENTITY = [0., 0., 1., 1., 0., 0., 0., 1., 0.]
FLOOR = -15.375
JEEP_POSITION = [453.5, FLOOR - (.040839687 - .75) + .01, -418.]
PLAYER_POSITION = [453.5, FLOOR + 2.7013111, -420.]
SYMBOLS = {
    'rf_scene_occupied_mover_fixture': 8,
    'rf_scene_occupied_mover_rows': 9 * 128,
    'rf_scene_occupied_mover_children': 9 * 4 * 64,
    'rf_scene_world_controller_steps': 8,
    'rf_scene_group_control_children': 12,
    'rf_scene_passive_attachment': 14,
    'rf_scene_vehicle_state': 16,
    'rf_scene_vehicle_selection': 12,
    'rf_scene_jeep_seats': 8,
    'rf_scene_jeep_entry_probe': 12,
    'rf_scene_vehicle_switch': 16,
    'rf_scene_setup_result': 4,
    'rf_scene_passive_roof_fixture': 10,
    'rf_scene_passive_npc_fixture': 10,
    'rf_scene_npc_support_lifecycle': 8,
    'rf_scene_enemy_combat': 8,
    'rf_scene_world_load_reject': 3,
    'rf_scene_checkpoint_world_reject': 9,
    'rf_scene_npc_checkpoint_reject_state': 6,
}


def replay(loaded=False):
    frames, uses = (LOAD_FRAMES, (30, 60)) if loaded else (FRAMES, (90, 120, 150))
    return b'RFI6' + U(48) + b''.join(struct.pack('<5f7I', 0, 0, 0, 0, 0,
        0, 0, int(frame in uses), 0, 0, 0, 0) for frame in range(frames))


def geometry_evidence(data):
    """Independent serialized face walk; no engine/PC executable is invoked."""
    geometry = inspect_geometry(data)
    vertices = [struct.unpack_from('<3f', data, geometry['vertices_offset'] + i * 12)
                for i in range(geometry['vertices'])]
    at = geometry['vertices_offset'] + geometry['vertices'] * 12 + 4
    floor, obstructing = None, []
    clear_min, clear_max = [450.5, -15.374, -422.], [457.5, -10.9, -414.]
    for index in range(geometry['faces']):
        header = data[at:at + 56]
        count, = struct.unpack_from('<I', header, 52)
        stride = 12 if struct.unpack_from('<I', header, 20)[0] == NO_HANDLE else 20
        at += 56
        points = [vertices[struct.unpack_from('<I', data, at + j * stride)[0]] for j in range(count)]
        at += count * stride
        low = [min(p[k] for p in points) for k in range(3)]
        high = [max(p[k] for p in points) for k in range(3)]
        if index == 78:
            floor = dict(index=index, room=struct.unpack_from('<I', header, 48)[0],
                flags=struct.unpack_from('<I', header, 40)[0],
                plane=list(struct.unpack_from('<4f', header)), minimum=low, maximum=high,
                points=points, header_sha256=sha(header))
        if all(high[k] > clear_min[k] and low[k] < clear_max[k] for k in range(3)):
            obstructing.append(index)
    require(floor is not None and floor['room'] == 10 and floor['flags'] == 0 and
        floor['plane'] == [0., 1., 0., 15.375] and
        floor['minimum'] == [448.5, FLOOR, -424.] and
        floor['maximum'] == [458.5, FLOOR, -404.] and len(floor['points']) == 8,
        'Original L20S2 hangar floor78 changed')
    require(not obstructing, 'Static faces enter Jeep/player/exit clear envelope: %r' % obstructing)
    return dict(source='levels2.vpp/L20S2.rfl static geometry', sha256=sha(data), floor=floor,
        clear_minimum=clear_min, clear_maximum=clear_max, intersecting_face_aabbs=obstructing,
        note='Conservative static-face AABB rejection above the floor; ordinary native entry/exit and save placement still must pass.')


def prepare_level(folder):
    folder = Path(folder)
    fixture, recipe = prepare_controls(folder, 'translation')
    base = read_entry(fixture, 'L20S2.rfl')
    parsed = inspect(io.BytesIO(base), dict(offset=0, size=len(base), name='L20S2.rfl'))
    original = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    eos = next(r for r in entity_rows(original) if r['uid'] == RIDER)
    require(eos['name'] == 'eos' and actor_details(eos) == dict(relationship=1, friendliness=2,
        health=1., armor=0., primary='none', secondary='none', seat_host_uid=-1, creation_flags=4),
        'Original friendly/unarmed Eos4716 changed')
    jeep_source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L12S1.rfl')
    jeep = next(r for r in entity_rows(jeep_source) if r['uid'] == JEEP)
    details = metadata(jeep)
    require(jeep['name'] == 'Jeep01' and details['seat_host_uid'] == -1 and not details['creation_flags'],
        'Original visible unattached Jeep7629 changed')
    raw = bytearray(jeep['raw'])
    raw[jeep['transform']:jeep['transform'] + 48] = struct.pack('<12f', *JEEP_POSITION, *DISK_IDENTITY)
    require(raw[:jeep['transform']] == jeep['raw'][:jeep['transform']] and
        raw[jeep['transform'] + 48:] == jeep['raw'][jeep['transform'] + 48:], 'Jeep non-pose metadata changed')
    geometry = geometry_evidence(section_payload(base, parsed, 0x100))
    model = model_geometry('Jeep01.v3m')
    bounds = [[min(JEEP_POSITION[k] + s['center'][k] - s['radius'] for s in model['spheres']),
               max(JEEP_POSITION[k] + s['center'][k] + s['radius'] for s in model['spheres'])]
              for k in range(3)]
    require(abs(bounds[1][0] - FLOOR - .01) < .000002, 'Jeep original wheel grounding changed')
    require(all(bounds[k][0] > geometry['clear_minimum'][k] and bounds[k][1] < geometry['clear_maximum'][k]
                for k in range(3)), 'Jeep actual hull does not fit clear hangar envelope')
    host = entity_rows(base)[0]
    records = [host['raw'], eos['raw'], bytes(raw)]
    replacements = {0x30000: U(3) + b''.join(records),
                    0x70000: struct.pack('<12f', *PLAYER_POSITION, *DISK_IDENTITY)}
    out, offsets = bytearray(base[:parsed['sections'][0]['offset']]), {}
    for section in parsed['sections']:
        kind = int(section['type'], 16)
        payload = replacements.get(kind, section_payload(base, parsed, kind))
        offsets[kind] = len(out)
        out += U(kind, len(payload)) + payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    check = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L20S2.rfl'))
    require(check['player_offset_matches'] and check['info_offset_matches'] and not check['trailing_bytes'],
        'Fixture level header failed round trip')
    require([r['raw'] for r in entity_rows(out)] == records, 'Retained owner records changed')
    for section in parsed['sections']:
        kind = int(section['type'], 16)
        if kind not in replacements:
            require(section_payload(base, parsed, kind) == section_payload(out, check, kind),
                'Attached-controls section changed: ' + hex(kind))
    archive(fixture, [('L20S2.rfl', out)])
    require(read_entry(fixture, 'L20S2.rfl') == out, 'Private archive failed round trip')
    recipe.update(scope=__doc__, status='PREPARED_NOT_RUNTIME_VALIDATED', frames=FRAMES,
        load_frames=LOAD_FRAMES, source_samples=SOURCE_FRAMES, load_samples=LOAD_SAMPLES,
        geometry=geometry, jeep=dict(uid=JEEP, source='levels2.vpp/L12S1.rfl', metadata=details,
            original_record_sha256=sha(jeep['raw']), record_sha256=sha(raw),
            original_pose=list(struct.unpack_from('<12f', jeep['raw'], jeep['transform'])),
            staged_pose=JEEP_POSITION + DISK_IDENTITY, model=model, hull_bounds=bounds),
        player=dict(original_pose=list(struct.unpack('<12f', section_payload(base, parsed, 0x70000))),
            staged_pose=PLAYER_POSITION + DISK_IDENTITY),
        rider=dict(uid=RIDER, record_sha256=sha(eos['raw']), details=actor_details(eos),
            setup='Existing campaign-passive-roof.bin mode9 at40; explicitly seeded, no natural landing claim'),
        timeline=dict(reveal=0, rider_seed=40, controller_start=START_FRAME,
            board=90, exit=120, reboard=150, final_physics=FRAMES - 2),
        load_timeline=dict(first_saved_observation=1, exit=30, reboard=60, final_physics=LOAD_FRAMES - 2),
        selection=dict(uid=JEEP, profile=3, source=2, override=False),
        archive_sha256=sha(fixture.read_bytes()), level_sha256=sha(out), limitations=__doc__)
    # The original control recipe's contact-operation schedule is not run here.
    for name in ('source_frames', 'source_operations', 'load_operations'):
        recipe.pop(name, None)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    (folder / 'player-replay.bin').write_bytes(replay())
    (folder / 'load-replay.bin').write_bytes(replay(True))
    return fixture, recipe


def source_inputs(fixture, recipe):
    return {'scene-fixture.vpp': fixture.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'),
        'scene-preview.flag': b'', 'campaign-spawn.flag': b'', 'player-control.flag': b'',
        'campaign-setup.bin': U(SHOW), 'campaign-passive-roof.bin': U(HOST, 40, 9),
        'campaign-occupied-movers.bin': U(0x4d4f4652, 1, FRAMES, 0),
        'player-replay.bin': replay(), 'world-hdd-save.flag': b'1'}


def rows(extra):
    main, child = extra['rf_scene_occupied_mover_rows'], extra['rf_scene_occupied_mover_children']
    return [(main[i * 128:(i + 1) * 128],
        [child[(i * 4 + j) * 64:(i * 4 + j + 1) * 64] for j in range(4)]) for i in range(9)]


def effective_velocity(row, recipe):
    # RFMC velocity[] is an unchanged wire field and stays zero for this
    # translation implementation. The actual speed/direction drive pending.
    keys = recipe['controller']['decoded']['keys']
    require(row[7:9] == [0, 1], 'Bounded controller left its first translation leg')
    direction = [keys[row[8]]['position'][k] - keys[row[7]]['position'][k] for k in range(3)]
    length = norm(direction)
    require(length > 0 and f(row[11]) > 0, 'Missing actual translation speed')
    return [value / length * f(row[11]) for value in direction]


def validate_owner_pose(row, children, recipe):
    require(row[1] == 0 and row[4] == KEY and row[6] == 1 and row[22] == HOST and
        row[24] == 1 and row[49] == row[57] == row[73] == row[118] == 1 and
        row[74:77] == [31, 31, 31], 'Owner/controller registration or child publication failed')
    require(row[40:42] == [JEEP, 3] and row[58] == RIDER and row[52] == 0 and row[117] == 1,
        'Independent Jeep, driver role or living rider UID changed')
    require(row[60] == row[23] and row[61] == 1 and f(row[62]) == 1.,
        'Friendly rider lost grounded mode or the living Fighter support')
    require([f(row[26]), f(row[27])] == [2000., 0.] and
        [f(row[100]), f(row[101])] == [recipe['jeep']['metadata']['authored_health'],
                                    recipe['jeep']['metadata']['authored_armor']], 'Authored vehicle vitals changed')
    for child, authored in zip(children, recipe['children']):
        require(child[0:2] == [authored['uid'], authored['kind']] and child[4:6] == [1, 1]
            and child[44] == row[5] and child[58:60] == [0, 0], 'Child UID/membership/registry changed')
        close_vector(vector(child, 8), authored['position'], .00002, 'authored child position')
        close_vector(vector(child, 11, 9), authored['basis'], .000002, 'authored child basis')
        position, basis = expected_pose(authored['position'], authored['basis'], child)
        for at in (20, 32):
            close_vector(vector(child, at), position, .0002, 'moving child position')
        for at in (23, 35):
            close_vector(vector(child, at, 9), basis, .000003, 'moving child basis')
        require(child[20:32] == child[32:44], 'Adapter/published child pose differs')
    require(row[85:97] == children[-1][32:44], 'Trigger live volume differs from controller publication')
    position, basis = expected_pose(recipe['host']['position'], recipe['host']['basis'], children[-1])
    close_vector(vector(row, 28), position, .0003, 'moving Fighter position')
    close_vector(vector(row, 31, 9), basis, .000003, 'moving Fighter basis')
    close_vector(vector(row, 13), vector(row, 16), 0, 'controller once-committed pose')
    if row[43]:
        require(row[44:46] == [row[46]] * 2 and row[47:49] == [row[42]] * 2 and
            row[114:116] == [row[42], row[46]], 'Occupied Jeep lost exact seated ownership')
    else:
        require(row[44:46] == [NO_HANDLE] * 2 and row[47:49] == [NO_HANDLE] * 2,
            'Ordinary exit failed to release player/host ownership')
    handles = [row[5], row[23], row[42], row[46], row[56], row[59], *[c[2] for c in children[:3]]]
    require(all(h not in (0, NO_HANDLE) for h in handles) and len(set(handles)) == len(handles),
        'Distinct full generation owner handles missing')
    return handles


def validate_phase(guest, recipe, saved=None):
    loaded = saved is not None
    frames, phase = (LOAD_FRAMES, 2) if loaded else (FRAMES, 1)
    require(guest['guest_phase'] == 5 and guest['frames'] == frames and
        guest['memory_bytes'] == 64 * 1024 * 1024 and guest['free_pages'] > 0 and
        not guest['player_life'][2], 'Incomplete live stock64MiB phase')
    x = guest['extra']
    require(x['rf_scene_occupied_mover_fixture'] == [1, phase, 9, 0 if loaded else 1, 0, frames - 2, 1, 9],
        'Missing exact fixture observations or unexpected start replay')
    require(x['rf_scene_vehicle_selection'] == [3, 1, 0, 0, 0, 0, 1, 1, JEEP, 3, 2, 0],
        'Normal metadata selection did not choose independent Jeep7629')
    require(not x['rf_scene_vehicle_state'][5] and not x['rf_scene_vehicle_switch'][1] and
        not x['rf_scene_enemy_combat'][2] and not x['rf_scene_enemy_combat'][7] and
        not x['rf_scene_passive_attachment'][13], 'Unexpected vehicle switch, combat or attachment error')
    adapter = x['rf_scene_group_control_children']
    require(adapter[:2] == [4, 4] and adapter[3] > 1 and adapter[5] > 0 and
        adapter[6:8] == [0, 0] and adapter[9] == 0, 'Four-owner moving control adapter failed')
    samples = rows(x)
    expected_frames = LOAD_SAMPLES if loaded else SOURCE_FRAMES
    identities, health = None, None
    observations = []
    for index, (row, children) in enumerate(samples):
        frame = expected_frames[index]
        occupied = int(frame < 30 or frame >= 60) if loaded else int(90 <= frame < 120 or frame >= 150)
        boards = int(frame >= 60) if loaded else int(frame >= 90) + int(frame >= 150)
        exits = int(frame >= (30 if loaded else 120))
        require(row[0] == frame and row[2] == phase and row[43] == occupied and
            row[50:52] == [boards, exits], 'Ordinary Use transition failed at frame%d: %r' %
                (frame, [row[43], *row[50:52]]))
        ids = validate_owner_pose(row, children, recipe)
        current_health = [child[7] for child in children[:3]]
        if identities is None:
            identities, health = ids, current_health
        require(ids == identities and current_health == health, 'Full handles or clutter health changed in phase')
        support_speed = effective_velocity(row, recipe) if f(row[10]) > 0 else [0., 0., 0.]
        close_vector(vector(row, 70), support_speed, .002, 'accepted NPC support velocity')
        counters = row[77:85]
        require(counters[0:3] == [frame + 1] * 3 and counters[3] + counters[4] == counters[0] and
            counters[5:] == [frame, 0, 0], 'Controller did not tick/commit exactly once per eligible step: %r' % counters)
        expected_occupied = (min(frame, 29) + max(0, frame - 59)) if loaded else \
            (max(0, min(frame, 119) - 89) + max(0, frame - 149))
        require(counters[3] == expected_occupied, 'Occupied world-step count disagrees with actual Use transitions')
        require(row[53] == (0 if loaded else 1) and row[54] == int(loaded or frame >= START_FRAME),
            'Setup/contact activation count replayed or failed')
        if not loaded and frame >= START_FRAME:
            close_vector([f(row[10])], [(frame - START_FRAME) / 60.], .000025, 'continuous source controller clock')
        if loaded:
            require(ids == saved['handles'] and current_health == saved['clutter_health_bits'],
                'Fresh process changed saved full handles or clutter health')
            close_vector([f(row[10])], [saved['controller_phase'] + frame / 60.], .00004,
                'saved controller clock plus normal post-load ticks')
        observations.append(dict(frame=frame, occupied=bool(occupied), boards=boards, exits=exits,
            steps=counters, controller_phase=f(row[10]), fighter_position=vector(row, 28),
            rider_position=vector(row, 64), jeep_position=vector(row, 102), trigger_position=vector(row, 85)))
    for index in range(1, len(samples)):
        before, after = samples[index - 1][0], samples[index][0]
        if not loaded and before[0] <= START_FRAME:
            continue  # Initial explicit placement settles before acceptance samples.
        dt = (after[0] - before[0]) / 60.
        displacement = [f(after[28 + k]) - f(before[28 + k]) for k in range(3)]
        close_vector(displacement, [value * dt for value in effective_velocity(after, recipe)], .0005,
            'one controller displacement through occupancy transition')
        rider_delta = [f(after[64 + k]) - f(before[64 + k]) for k in range(3)]
        # Ground fit may adjust Y; horizontal carry has no second producer.
        close_vector([rider_delta[k] for k in (0, 2)], [displacement[k] for k in (0, 2)],
            .0005, 'single horizontal NPC carry through occupancy transition')
        close_vector([rider_delta[1]], [displacement[1]], .035, 'NPC vertical carry and ground fit')
    first, last = samples[0][0], samples[-1][0]
    require(norm([f(last[28 + k]) - f(first[28 + k]) for k in range(3)]) > .5,
        'Separate controller/host did not continue moving')
    require(x['rf_scene_world_controller_steps'] == last[77:85], 'Final physics snapshot does not match controller totals')
    if loaded:
        require(guest['checkpoint_state'][8] == 1 and guest['checkpoint_state'][0] == 0 and
            not any(x['rf_scene_world_load_reject']) and not any(x['rf_scene_setup_result']) and
            not any(x['rf_scene_passive_roof_fixture']), 'Fresh load failed or replayed setup/rider placement')
        close_vector(vector(first, 13), [saved['controller_pending'][k] + saved['effective_velocity'][k] / 60.
            for k in range(3)], .0002, 'loaded controller plus one tick')
        close_vector(vector(first, 28), [saved['fighter_position'][k] + saved['effective_velocity'][k] / 60.
            for k in range(3)], .0003, 'loaded Fighter plus one tick')
        expected_rider = [saved['rider_position'][k] + saved['effective_velocity'][k] / 60. for k in range(3)]
        close_vector([f(first[64 + k]) for k in (0, 2)], [expected_rider[k] for k in (0, 2)],
            .0005, 'saved NPC horizontal carry plus one tick')
        close_vector([f(first[65])], [expected_rider[1]], .035, 'saved NPC vertical carry and ground fit')
    else:
        require(x['rf_scene_setup_result'] == [1, SHOW, 50, 0], 'Source ordinary UnHide setup differed')
        roof = x['rf_scene_passive_roof_fixture']
        require(roof[:2] == [HOST, 40] and roof[2] == 1 and roof[9] == 9,
            'Source did not perform exactly one existing NPC roof placement')
    return dict(result='PASS', free_pages=guest['free_pages'], handles=identities,
        clutter_health_bits=health, observations=observations)


def validate_saved(guest, payload, recipe, source_validation):
    state = guest['checkpoint_state']
    require(state[9] == 1 and state[3] == 0 and state[4] == len(payload), 'Ordinary occupied source save failed')
    sections = checkpoint_sections(payload)
    mover, clutter, trigger, vehicle = sections[3], sections[8], sections[5], sections[11]
    component(mover, b'RFMC', 1)
    component(clutter, b'RFPC', 1)
    component(trigger, b'RFTC', 1)
    require(len(mover) == 152 and struct.unpack_from('<I', mover, 16)[0] == 1, 'Expected one RFMC1 controller')
    m = mover[64:]
    uid, kind, keys, flags, mode = struct.unpack_from('<5I', m)
    require((uid, kind, keys, mode) == (KEY, 1, 4, 1), 'Saved mover identity/type changed')
    phase, speed, distance = struct.unpack_from('<3f', m, 40)
    last, children = rows(guest['extra'])[-1]
    close_vector([phase, speed, distance], vector(last, 10), .000001, 'saved final controller clock')
    require(0 < phase < DURATION, 'Controller was not saved during motion')
    pending = list(struct.unpack_from('<3f', m, 64))
    velocity = list(struct.unpack_from('<3f', m, 76))
    close_vector(pending, vector(last, 13), 0, 'saved exact controller pending')
    close_vector(velocity, vector(last, 19), 0, 'saved exact controller velocity')
    require(struct.unpack_from('<I', clutter, 16)[0] == 3 and len(clutter) == 136, 'Expected three RFPC1 rows')
    for index, child in enumerate(children[:3]):
        saved = struct.unpack_from('<6I', clutter, 64 + index * 24)
        require(saved[0] == child[0] and saved[2] == child[7] and saved[3] == (child[6] & 0x204002),
            'Saved clutter UID/health/flags mismatch')
    require(struct.unpack_from('<2I', trigger, 16) == (1, 1) and len(trigger) == 168,
        'Expected one unchanged RFTC1 trigger')
    t = struct.unpack_from('<10I', trigger, 128)
    require(t[:3] == (0, TRIGGER, 1) and t[3] == (last[55] & ~64) and t[4] == 1,
        'Saved actual trigger count/flags mismatch')
    require(vehicle[:4] == b'RFVA' and struct.unpack_from('<3I', vehicle, 4) == (2, 160, 1)
        and len(vehicle) == 256, 'Expected RFVA2 attached Fighter plus active Jeep RFVC3')
    active, passive = vehicle[16:176], vehicle[176:]
    component(active, b'RFVC', 3)
    a = list(struct.unpack('<40I', active))
    require(a[4:6] == [3, 3] and a[27] == 0 and a[8:11] == guest['extra']['rf_scene_vehicle_state'][6:9],
        'Saved Jeep is not the actual living occupied driver')
    require(struct.unpack_from('<2I', passive) == (HOST, 1) and
        struct.unpack_from('<2f', passive, 56) == (2000., 0.), 'Saved passive Fighter identity/vitals changed')
    close_vector(list(struct.unpack_from('<3f', passive, 8)), vector(last, 28), 0, 'saved exact Fighter position')
    close_vector(list(struct.unpack_from('<9f', passive, 20)), vector(last, 31, 9), 0, 'saved exact Fighter basis')
    return dict(formats=['RFWC%d' % struct.unpack_from('<I', payload, 4)[0], 'RFMC1', 'RFPC1', 'RFTC1', 'RFVA2', 'RFVC3'],
        bytes=len(payload), payload_sha256=sha(payload), controller_phase=phase,
        controller_pending=pending, controller_wire_velocity=velocity,
        effective_velocity=effective_velocity(last, recipe), fighter_position=vector(last, 28),
        rider_position=vector(last, 64), jeep_position=vector(a, 8),
        handles=source_validation['handles'], clutter_health_bits=source_validation['clutter_health_bits'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    args = parser.parse_args()
    if args.prepare_only:
        fixture, recipe = prepare_level(args.prepare_only)
        print(json.dumps(dict(status=recipe['status'], fixture=str(fixture), archive_sha256=recipe['archive_sha256'],
            jeep_pose=recipe['jeep']['staged_pose'], geometry=recipe['geometry']), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    require(base.is_file(), 'Missing isolated test HDD base')
    folder = ROOT / 'artifacts/xemu' / ('vehicle-occupied-movers-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    fixture, recipe = prepare_level(folder / 'level')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {
        'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', 'campaign-occupied-movers.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in names}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe, original_input_sha256={
        name: sha(data) if data is not None else None for name, data in original.items()})
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        inputs = source_inputs(fixture, recipe)
        for name, data in inputs.items():
            (DISC / name).write_bytes(data)
        require(all((DISC / name).read_bytes() == data for name, data in inputs.items()), 'Source input staging mismatch')
        report['source_input_sha256'] = {name: sha(data) for name, data in inputs.items()}
        from xemu_world_hdd import prepare
        hdd = prepare(ROOT, base)
        report['isolated_hdd'] = str(hdd)
        build(folder, 'source')
        report['source_binary_evidence'] = preserve_launch_binaries(folder / 'source-binaries')
        source = run_guest(folder, 'source', hdd, FRAMES, 600, capture_world=True,
            extra_symbols=SYMBOLS, allow_guest_error=True)
        report['source'] = source
        report['source_validation'] = validate_phase(source, recipe)
        payload = (folder / 'source/xbox-world.rfwc').read_bytes()
        report['saved'] = validate_saved(source, payload, recipe, report['source_validation'])
        for name in ('campaign-setup.bin', 'campaign-setup-immediate.flag', 'campaign-passive-roof.bin', 'world-hdd-save.flag'):
            (DISC / name).unlink(missing_ok=True)
        inputs = {'scene-preview.flag': b'', 'world-hdd-load.flag': b'1',
            'campaign-occupied-movers.bin': U(0x4d4f4652, 2, LOAD_FRAMES, 0), 'player-replay.bin': replay(True)}
        for name, data in inputs.items():
            (DISC / name).write_bytes(data)
        require(all((DISC / name).read_bytes() == data for name, data in inputs.items()), 'Load input staging mismatch')
        report['load_input_sha256'] = {name: sha((DISC / name).read_bytes()) for name in names if (DISC / name).is_file()}
        build(folder, 'load')
        report['load_binary_evidence'] = preserve_launch_binaries(folder / 'load-binaries')
        loaded = run_guest(folder, 'load', hdd, LOAD_FRAMES, 540, snapshot=True,
            extra_symbols=SYMBOLS, allow_guest_error=True)
        report['load'] = loaded
        report['load_validation'] = validate_phase(loaded, recipe, report['saved'])
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
            report['restored_input_sha256'] = {name: sha((DISC / name).read_bytes()) if (DISC / name).exists() else None for name in names}
            report['disc_restored'] = all(((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
                for name, data in original.items())
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(folder, report['result'], flush=True)
        require(report['disc_restored'], 'Original staged inputs including scene-preview.flag were not restored')


if __name__ == '__main__':
    main()
