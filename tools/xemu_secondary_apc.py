"""Bounded original-road Xbox secondary APC ownership and ordinary saves.

Original L1S3 APC26, APC9627, Goto9517, Remove_Object9626 and UnHide9628 retain
all authored bytes. Original geometry and navigation stay intact. An isolated
Delay graph starts the real Goto, retires26 and reveals9627 later, then tests
UnHide of the retired owner. Other actors, triggers and controller setup are
removed. The player stays idle; this is no campaign route or full-route claim.
Three stock64MiB boots prove moving save/fresh-load, continued independent
motion, retirement save/fresh-load and no revival. Native observations and
isolated collision/admission audits preserve live state. No images, host
input, guest writes, PC runtime or fabricated saves.

--prepare-only writes and independently decodes private fixture inputs.
The parent owns all builds, emulator runs, input restoration and cleanup.
"""

import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from inspect_navigation_records import inspect as inspect_navigation
from xemu_npc_actor_interception import command
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import actor_details, event_rows, section_payload
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd

SECONDARY, SELECTED, GOTO, REMOVE, SHOW = 26, 9627, 9517, 9626, 9628
START, RETIRE_TIMER, REVIVE_TIMER, REVIVE = range(916700, 916704)
NONE, HIDDEN, TIMER_PERIOD = 0xffffffff, 0x4000, 1072800000
CONFIG = 'campaign-secondary-apc.bin'
SOURCE_FRAMES, RETIRE_FRAMES, RETIRED_FRAMES = 120, 160, 120
SAMPLE_FRAMES = (0, 20, 40, 60, 78, 80, 100, 118, 119, 120, 138, 158, 159)
ROW_WORDS = 256
EVENT_UIDS = (GOTO, REMOVE, SHOW, START, RETIRE_TIMER, REVIVE_TIMER, REVIVE)
OBSERVATION = 'rf_scene_secondary_vehicle_samples'
SYMBOLS = {OBSERVATION: len(SAMPLE_FRAMES) * ROW_WORDS,
    'rf_scene_secondary_vehicle_fixture': 8, 'rf_scene_secondary_vehicle_clocks': len(SAMPLE_FRAMES), 'rf_scene_secondary_vehicle_collision_audit': 56, 'rf_scene_secondary_vehicle': 16,
    'rf_scene_secondary_vehicle_checkpoint': 8, 'rf_scene_secondary_vehicle_checkpoint_audit': 16,
    'rf_scene_secondary_vehicle_restore': 80, 'rf_scene_secondary_vehicle_clearance': 6, 'rf_scene_setup_result': 4,
    'rf_scene_vehicle_state': 16, 'rf_scene_vehicle_damage': 8,
    'rf_scene_passive_damage': 8, 'rf_scene_enemy_combat': 8,
    'rf_scene_profile_stage': 2, 'rf_animation_progress': 4,
    'rf_scene_vehicle_route_state': 8, 'rf_scene_world_load_reject': 3,
    'rf_scene_checkpoint_world_reject': 9, 'rf_scene_script_slays': 6,
    'rf_scene_vehicle_wreck_exit': 12, 'rf_scene_active_vehicle_visibility': 8,
    'rf_scene_vehicle_visibility': 8}
LIMITATIONS = ('One unoccupied original secondary APC on the first authored road segment; '
    'selected APC9627 remains independently owned and stationary. No natural concurrent '
    'driving, player boarding, passengers, full-route completion, campaign progression '
    'or visual/audio claim. Collision blocker audit uses an isolated candidate query.')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def replay(phase):
    frames = {1: SOURCE_FRAMES, 2: RETIRE_FRAMES, 3: RETIRED_FRAMES}[phase]
    return b'RFI6' + U(48) + struct.pack('<5f7I', *([0] * 12)) * frames


def prepare_level(folder):
    folder.mkdir(parents=True, exist_ok=True)
    source = read_entry(ROOT / 'Installed_Game/levels1.vpp', 'L1S3.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L1S3.rfl'))
    require(meta['version'] == 180 and not meta['trailing_bytes'], 'Incomplete original L1S3')
    entities = {row['uid']: row for row in entity_rows(source)}
    owners = [entities[uid] for uid in (SECONDARY, SELECTED)]
    details = {str(row['uid']): actor_details(row) for row in owners}
    require(all(row['name'] == 'APC' for row in owners) and
        details[str(SECONDARY)]['health'] == 900. and details[str(SECONDARY)]['creation_flags'] == 0 and
        details[str(SELECTED)]['health'] == 5000. and details[str(SELECTED)]['creation_flags'] == 2 and
        all(value['seat_host_uid'] == -1 for value in details.values()), 'Original APC identity or admission changed')
    original_events = {row['uid']: row for row in event_rows(section_payload(source, meta, 0x600))}
    for uid, kind, links in ((GOTO, 'Goto', [SECONDARY]), (REMOVE, 'Remove_Object', [SECONDARY]),
                              (SHOW, 'UnHide', [SELECTED])):
        require(original_events[uid]['type'] == kind and original_events[uid]['links'] == links,
                'Original event identity/target changed: ' + str(uid))
    navigation = inspect_navigation(section_payload(source, meta, 0x20000))
    require(len(navigation) > 99 and navigation[41]['uid'] == 280 and navigation[99]['uid'] == 9390 and
        99 in navigation[41]['neighbors'], 'Original first280→9390 graph edge changed')
    goto_position = list(struct.unpack_from('<3f', original_events[GOTO]['raw'], 6 + len('Goto')))
    require(goto_position == [-41.3365478515625, 30.499998092651367, 28.113662719726562],
        'Original Goto fixed point changed')
    # The no-revival probe is a declared clone of genuine UnHide9628: only its
    # fixture UID and target change. The original9628 record stays byte-exact.
    revive = bytearray(original_events[SHOW]['raw'])
    struct.pack_into('<I', revive, 0, REVIVE)
    struct.pack_into('<I', revive, original_events[SHOW]['links_at'] + 4, SECONDARY)
    events = [original_events[uid]['raw'] for uid in (GOTO, REMOVE, SHOW)] + [
        command(START, 'Delay', 'start_secondary_apc_check', (GOTO, RETIRE_TIMER, REVIVE_TIMER)),
        command(RETIRE_TIMER, 'Delay', 'retire_original_apc', (REMOVE, SHOW), delay=4.),
        command(REVIVE_TIMER, 'Delay', 'retired_unhide_check', (REVIVE,), delay=6.), bytes(revive)]
    replacements = {0x30000: U(2) + b''.join(row['raw'] for row in owners),
        0x600: U(len(events)) + b''.join(events), 0x3000: U(0), 0x40000: U(0),
        0x50000: U(0), 0x60000: U(0)}
    # Navigation0x20000 and waypoints0x10000 are deliberately preserved;
    # triggers0x60000 are removed to prevent any campaign progression.
    out, offsets, preserved = bytearray(source[:meta['sections'][0]['offset']]), {}, {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        data = section_payload(source, meta, kind)
        payload = replacements.get(kind, data)
        if kind not in replacements:
            preserved[hex(kind)] = sha(data)
        offsets[kind] = len(out)
        out += U(kind, len(payload)) + payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    checked = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L1S3.rfl'))
    require(checked['player_offset_matches'] and checked['info_offset_matches'] and not checked['trailing_bytes'],
            'Fixture section/header roundtrip failed')
    require([row['raw'] for row in entity_rows(out)] == [row['raw'] for row in owners],
            'Fixture changed an original owner record')
    decoded = event_rows(section_payload(out, checked, 0x600))
    require([row['raw'] for row in decoded] == [bytes(row) for row in events] and
            [row['uid'] for row in decoded] == list(EVENT_UIDS), 'Fixture event roundtrip failed')
    require(struct.unpack_from('<I', section_payload(source, meta, 0x20000))[0] > 0,
            'Original nonempty navigation graph is required')
    for kind in (0x100, 0x20000, 0x10000):
        require(section_payload(source, meta, kind) == section_payload(out, checked, kind),
                'Original geometry/navigation/waypoints changed: ' + hex(kind))
    for kind, digest in preserved.items():
        require(sha(section_payload(out, checked, int(kind, 16))) == digest,
                'Fixture changed preserved section ' + kind)
    path = folder / 'scene-fixture.vpp'
    archive(path, [('L1S3.rfl', out)])
    recipe = dict(scope=__doc__, source_sha256=sha(source), fixture_sha256=sha(bytes(out)),
        archive_sha256=sha(path.read_bytes()), preserved_sections=preserved, details=details,
        owners=[dict(uid=row['uid'], sha256=sha(row['raw']), position=list(struct.unpack_from('<3f', row['raw'], row['transform'])),
            pose_bits=list(struct.unpack_from('<12I', row['raw'], row['transform']))) for row in owners],
        events=[dict(uid=row['uid'], type=row['type'], links=row['links'], sha256=sha(row['raw'])) for row in decoded],
        original_event_sha256={str(uid): sha(original_events[uid]['raw']) for uid in (GOTO, REMOVE, SHOW)},
        navigation_count=len(navigation),
        first_nav_nodes=[dict(index=i, uid=navigation[i]['uid'], position=list(struct.unpack('<3f', navigation[i]['position'])))
            for i in (41, 99)], expected_first_nav_uid=9390, expected_start_nav_uid=280,
        expected_goal=[-41.3365478515625, 30.499998092651367, 28.113662719726562],
        source_frames=SOURCE_FRAMES, retire_frames=RETIRE_FRAMES, retired_frames=RETIRED_FRAMES,
        retirement_seconds=4., no_revival_seconds=6., observation_frames=SAMPLE_FRAMES,
        staged_fields=['filter entities to original26 and9627', 'isolated Delay links/setup',
            'remove unrelated controller, trigger, item and clutter records',
            'clone genuine UnHide9628 as fixture916703 targeting26'], limitations=LIMITATIONS)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    for phase, label in ((1, 'source'), (2, 'retire'), (3, 'retired')):
        (folder / (label + '-replay.bin')).write_bytes(replay(phase))
    return path, recipe


def phase_inputs(level, phase):
    inputs = {'scene-fixture.vpp': level.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L1S3.rfl'.ljust(64, b'\0'),
        'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
        CONFIG: U(0x43504153, phase, 0, 0), 'player-replay.bin': replay(phase)}
    if phase == 1:
        inputs['campaign-setup.bin'] = U(START)
    if phase != 1:
        inputs['world-hdd-load.flag'] = b'1'
    if phase != 3:
        inputs['world-hdd-save.flag'] = b'1'
    return inputs


def observed(guest, frame):
    at = SAMPLE_FRAMES.index(frame) * ROW_WORDS
    row = guest['extra'][OBSERVATION][at:at + ROW_WORDS]
    require(len(row) == ROW_WORDS and row[:2] == [1, frame] and not row[3],
            f'Missing/error exact native sample{frame}: {row[:4]}')
    return row


def event_observations(row):
    return {row[at]: row[at:at + 12] for at in range(160, 244, 12)}


def describe(row):
    wire = row[16:80]
    return dict(frame=row[1], secondary=dict(uid=row[4], handle=row[5], retired=row[12],
        health=f(row[254]), armor=f(row[255]), flags=row[13], position=[f(v) for v in wire[12:15]],
        velocity=[f(v) for v in wire[24:27]], accepted_velocity=[f(v) for v in wire[56:59]],
        route=dict(active=wire[6], event=wire[7], index=wire[8], retry=wire[9], count=wire[10],
            ordinals=wire[45:49], target=[f(v) for v in wire[36:39]], cost=f(wire[49]))),
        selected=dict(uid=row[6], handle=row[7], health=f(row[148]), armor=f(row[149]),
            hidden=bool(row[150] & HIDDEN), position=[f(v) for v in row[114:117]]),
        events={str(uid): dict(handle=v[1], type=v[2], remaining_ms=-1 if v[4] == NONE else v[4],
            actor=v[5], source=v[6], flags=v[7], mode=v[8], unhide_remaining_ms=-1 if v[9] == NONE else v[9],
            unhide_on=v[10], unhide_off=v[11]) for uid, v in event_observations(row).items()},
        steps=row[244], route_requests=row[245], route_found=row[246], route_misses=row[247],
        node_advances=row[248], removals=row[249], secondary_last_frame=row[250])


def check_identity(row, retired):
    wire = row[16:80]
    require(row[4] == SECONDARY and row[6] == SELECTED and row[8:12] == [1, 11, 1, 1] and
        len({row[5], row[7], row[158]}) == 3 and all(value not in (0, NONE) and value >> 16
        for value in (row[5], row[7], row[158])), 'UID/full handle or published pose identity mismatch')
    require(wire[:3] == [SECONDARY, 2, int(retired)] and row[12] == retired and
        row[15] == row[152] == wire[55] == 0 and row[153] == 1 and row[154] == NONE and
        row[155] == 0 and row[159] == NONE, 'Retirement fabricated death or occupied either owner')
    require(f(row[254]) == f(wire[50]) == 900. and f(row[255]) == f(wire[51]) and
        f(row[148]) == 5000. and f(row[149]) == 0., 'Authored owner health changed')
    require(row[13] == wire[52] and row[150] == row[151] and
        bool(row[13] & 2) == retired and bool(row[13] & HIDDEN) == retired,
        'Owner retired/hidden flags disagree')
    require(wire[7] == GOTO and wire[6] == int(not retired), 'Original route event/active ownership changed')
    require(wire[36:39] == list(struct.unpack('<3I', struct.pack('<3f',
        -41.3365478515625, 30.499998092651367, 28.113662719726562))), 'Original Goto fixed point changed')
    if row[1] or row[2] != 1:
        require(wire[10] >= 2 and 1 <= wire[8] < wire[10] <= 4 and wire[9] <= 60 and
            wire[45 + wire[8]] == 100, 'Original graph order/cursor no longer targets nav9390')
    require(1 <= row[252] <= 8 and 1 <= row[253] <= 8 and
        all(math.isfinite(f(v)) for v in wire[12:45] + wire[49:52] + wire[56:59] + row[80:113] + row[114:147]),
        'Missing actual rigid hull or nonfinite state')
    require(wire[12:30] == row[80:98] and wire[30:36] == row[107:113],
        'Serialized pose/velocity/momentum/forces do not match complete rigid state')
    if retired:
        require(not any(wire[24:36]) and not any(wire[56:59]), 'Retired owner retained motion/forces')


def movement(a, b):
    return math.hypot(f(b[28]) - f(a[28]), f(b[30]) - f(a[30]))


def check_guest(guest, phase, previous=None, payload=None):
    frames = {1: SOURCE_FRAMES, 2: RETIRE_FRAMES, 3: RETIRED_FRAMES}[phase]
    require(guest['guest_phase'] == 5 and guest['frames'] == frames and
        guest['memory_bytes'] == 64 * 1024 * 1024 and guest['free_pages'] > 0 and
        not guest['player_life'][2], 'Incomplete living stock64MiB phase')
    x, state = guest['extra'], guest['checkpoint_state']
    require(x['rf_scene_secondary_vehicle_fixture'] ==
        [1, phase, sum(frame < frames for frame in SAMPLE_FRAMES), SECONDARY, SELECTED, 1, int(phase == 1), 0],
        'Native opt-in/sample count/admission audit failed')
    require(not x['rf_scene_secondary_vehicle'][13] and not x['rf_scene_vehicle_state'][5] and
        not any(x['rf_scene_vehicle_damage'][i] for i in (2, 3)) and
        not any(x['rf_scene_passive_damage'][i] for i in (2, 3)) and
        not any(x['rf_scene_enemy_combat'][i] for i in (2, 7)) and
        not any(x['rf_scene_script_slays']) and not any(x['rf_scene_vehicle_wreck_exit']) and
        not any(x['rf_scene_vehicle_route_state'][i] for i in (0, 3, 5, 7)),
        'Unexpected errors, damage/death effects, combat or selected-owner route')
    require(not any(guest['level_transitions']) and not any(guest['level_request']),
        'Unexpected campaign transition/request')
    if phase == 1:
        require(x['rf_scene_setup_result'] == [1, START, 48, 0], 'Source setup did not dispatch only isolated Delay graph')
    else:
        require(state[8] == 1 and state[0] == 0 and state[1] == previous['bytes'] and
            not any(x['rf_scene_setup_result']) and not any(x['rf_scene_world_load_reject']),
            'Fresh load rejected state or replayed setup')
    if phase != 3:
        require(payload is not None and state[9] == 1 and not state[3] and state[4] == len(payload),
                'Ordinary source/retirement save failed')
    else:
        require(state[9] == 0, 'Retired continuation unexpectedly wrote another save')


def check_checksum(data, label):
    actual = 2166136261
    for at, value in enumerate(data):
        actual = ((actual ^ (0 if 12 <= at < 16 else value)) * 16777619) & NONE
    require(actual == struct.unpack_from('<I', data, 12)[0], label + ' checksum mismatch')


def saved_rows(payload, final_world, final_selected, retired):
    sections = checkpoint_sections(payload)
    data = sections[11]
    require(data[:4] == b'RFPV' and len(data) >= 16, 'Missing RFPV2 visibility/physics wrapper')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require((version, count) == (2, 2) and len(data) == 16 + inner + count * 72,
            'Unexpected RFPV2 owners or length')
    physics = [list(struct.unpack_from('<18I', data, 16 + inner + i * 72)) for i in range(count)]
    require([row[:2] for row in physics] == [[SELECTED, 0], [SECONDARY, 1]], 'RFPV owners changed')
    data = data[16:16 + inner]
    require(data[:4] == b'RFSV' and len(data) >= 16, 'Missing independent RFSV secondary state')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require((version, count) == (1, 1) and len(data) == 16 + inner + 256, 'Unexpected RFSV1 envelope')
    wire = list(struct.unpack_from('<64I', data, 16 + inner))
    require(wire == final_world[16:80] and wire[:3] == [SECONDARY, 2, int(retired)],
            'Ordinary secondary save differs from exact final world observation')
    require(physics[1][8:11] == wire[56:59] and physics[1][3] == wire[4] and
        physics[1][5] == wire[52] & 0x06004000 and physics[0][5] == final_selected[150] & 0x06004000,
        'RFPV independent velocity/flags disagree with RFSV and selected owner')
    data = data[16:16 + inner]
    require(data[:4] == b'RFVA' and len(data) >= 16, 'Missing original passive owner state')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require((version, count) == (2, 1) and len(data) == 16 + inner + 80, 'Unexpected RFVA2 envelope')
    passive = list(struct.unpack_from('<20I', data, 16 + inner))
    require(passive[:2] == [SECONDARY, 0] and passive[2:14] == wire[12:24] and
        passive[14:20] == wire[50:56], 'RFVA published owner differs from RFSV authoritative owner')
    active = data[16:16 + inner]
    require(active[:4] == b'RFVC' and len(active) in (128, 168) and
        struct.unpack_from('<2I', active, 4) == (2, 128) and
        struct.unpack_from('<2I', active, 16) == (2, 1), 'Selected APC gained route/occupancy')
    check_checksum(active[:128], 'RFVC2')
    active_words = list(struct.unpack_from('<32I', active))
    require(active_words[6:8] == final_selected[148:150] and active_words[8:20] == final_selected[114:126],
        'Selected independent health/pose differs from exact source observation')
    if len(active) == 168:
        require(active[128:132] == b'RFVR' and list(struct.unpack_from('<9I', active, 132)) == [3] + [0] * 8,
                'Selected APC received secondary route state')
    event_data = sections[4]
    require(len(event_data) == len(EVENT_UIDS) * 192, 'Event checkpoint count changed')
    native = event_observations(final_world)
    events = {}
    for at in range(0, len(event_data), 192):
        raw = event_data[at:at + 192]
        require(raw[:4] == b'RFEC' and struct.unpack_from('<2I', raw, 4) == (3, 192), 'Missing RFEC3 state')
        check_checksum(raw, 'RFEC3')
        words = list(struct.unpack('<48I', raw)); uid = words[4]
        require(uid in native and uid not in events, 'Unexpected/duplicate saved event')
        row = native[uid].copy()
        require(row[2] == words[5] and row[7:9] == words[16:18] and
            not words[18] and not words[28], 'Saved event state changed or retired an event')
        row[4] = words[40]
        if row[2] == 50:
            row[9:12] = [words[39], words[41], words[42]]
        events[str(uid)] = row
    require(set(map(int, events)) == set(EVENT_UIDS), 'Saved event identities changed')
    return dict(bytes=len(payload), sha256=sha(payload), wire=wire, physics=physics, passive=passive,
        active_pose=active_words[8:20], active_vitals=active_words[6:8],
        full_rigid=final_world[80:114], selected_rigid=final_selected[114:148],
        handles=[final_world[5], final_world[7], final_world[158]], events=events,
        formats=['RFWC', 'RFPV2', 'RFSV1', 'RFVA2', 'RFVC2', 'RFEC3'])


def check_loaded_exact(guest, previous):
    first = observed(guest, 0)
    require(first[16:80] == previous['wire'] and first[80:114] == previous['full_rigid'] and
        first[114:126] == previous['active_pose'] and first[148:150] == previous['active_vitals'] and
        [first[5], first[7], first[158]] == previous['handles'],
        'Fresh frame0 did not restore exact independent owner/rigid/selected state')
    restore = guest['extra']['rf_scene_secondary_vehicle_restore']
    require(restore[:3] == [1, SECONDARY, first[5]] and restore[4:8] ==
        [2, previous['wire'][2], GOTO, previous['wire'][6]] and restore[8:72] == previous['wire'],
        'Immutable RFSV assignment differs from actual saved row')
    clock = guest['extra']['rf_scene_secondary_vehicle_clocks'][0]
    require(0 <= clock <= TIMER_PERIOD, 'Invalid exact native observation clock')
    expected = {}
    for uid, values in previous['events'].items():
        row = values.copy()
        for at in (4, 9):
            if row[at] == NONE:
                continue
            delta = row[at] - clock  # Ordinary fresh restore anchors deadlines at0.
            if delta > TIMER_PERIOD // 2:
                delta -= TIMER_PERIOD
            elif delta < -TIMER_PERIOD // 2:
                delta += TIMER_PERIOD
            row[at] = delta & NONE
        expected[int(uid)] = row
    require(event_observations(first) == expected,
        'Fresh load changed exact saved event identity/state or clock-adjusted remaining timers')


def validate_source(guest, payload, recipe):
    check_guest(guest, 1, payload=payload)
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES if frame < SOURCE_FRAMES}
    for row in rows.values():
        check_identity(row, False)
        require(row[150] & HIDDEN, 'Selected original hidden APC revealed before retirement')
    require(len({(row[5], row[7], row[158]) for row in rows.values()}) == 1, 'Source full identity changed')
    for row in rows.values():
        require(math.hypot(f(row[114]) - f(rows[0][114]), f(row[116]) - f(rows[0][116])) < .01 and
            max(abs(f(a) - f(b)) for a, b in zip(row[117:126], rows[0][117:126])) < .01,
            'Selected APC moved with the secondary route')
    require(movement(rows[40], rows[118]) > .5 and f(rows[118][28]) < f(rows[40][28]) and
        abs(f(rows[118][30]) - recipe['owners'][0]['position'][2]) < 4.,
        'Source did not progress along original first280→9390 road')
    require(math.hypot(f(rows[118][40]), f(rows[118][42])) > .05, 'Source checkpoint was not mid-motion')
    audit = guest['extra']['rf_scene_secondary_vehicle_checkpoint_audit']
    require(audit[0] == 1 and audit[1] == 17 and audit[2] == (1 << audit[1]) - 1 and
        not audit[3] and not audit[4] and audit[6:8] == [1, 1],
        'Native malformed/legacy/no-mutation admission audit failed: ' + str(audit))
    saved = saved_rows(payload, rows[118], rows[119], False)
    require(saved['events'][str(RETIRE_TIMER)][4] not in (0, NONE) and
        saved['events'][str(REVIVE_TIMER)][4] > saved['events'][str(RETIRE_TIMER)][4],
        'Source ordinary timer graph did not retain retirement and later no-revival probe')
    return dict(result='PASS', saved=saved, malformed_audit=audit,
        observations={str(frame): describe(row) for frame, row in rows.items()})


def check_collision(audit):
    require(audit[:3] == [1, 138, SECONDARY] and audit[4] == SELECTED and audit[3] != audit[5] and
        audit[6:9] == [0, 1, audit[5]] and 0 <= f(audit[9]) < 1 and
        1 <= audit[12] <= 8 and 1 <= audit[13] <= 8 and 0 < f(audit[14]) and 0 < f(audit[15]) and
        audit[37:41] == [31, 1, 1, 4] and f(audit[41]) == 1. and audit[48] == 1,
        'Actual other-chassis owner-qualified candidate query failed: ' + str(audit))
    require(all(math.isfinite(f(v)) for v in audit[14:37] + audit[42:48]), 'Nonfinite candidate query evidence')
    return dict(query_uid=SECONDARY, query_handle=audit[3], blocker_uid=SELECTED, blocker_handle=audit[5],
        fraction=f(audit[9]), source_sphere_index=audit[10], target_sphere_index=audit[11],
        source_sphere_count=audit[12], target_sphere_count=audit[13], unchanged_mask=audit[37],
        query_start=[f(v) for v in audit[16:19]], query_end=[f(v) for v in audit[19:22]],
        provenance='One real source collision sphere; composed original world and actual visible other hull')


def validate_loaded(guest, previous, recipe, phase, payload=None):
    check_guest(guest, phase, previous, payload)
    check_loaded_exact(guest, previous)
    frames = RETIRE_FRAMES if phase == 2 else RETIRED_FRAMES
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES if frame < frames}
    for row in rows.values():
        check_identity(row, bool(row[12]))
        require([row[5], row[7], row[158]] == previous['handles'], 'Fresh continuation changed a full identity')
        require(bool(row[150] & HIDDEN) == (not bool(row[12])), 'Retirement/reveal changed different owners')
    result = dict(result='PASS', source_payload_sha256=previous['sha256'],
        observations={str(frame): describe(row) for frame, row in rows.items()})
    if phase == 2:
        require(rows[0][12] == rows[100][12] == 0 and rows[138][12] == rows[158][12] == 1 and
            movement(rows[0], rows[100]) > .5 and f(rows[100][28]) < f(rows[0][28]),
            'Fresh continuation did not resume road motion then retire')
        require(rows[138][249] == 1 and rows[158][16:80] == rows[138][16:80],
            'Retired owner kept stepping or repeated retirement')
        visibility = guest['extra']['rf_scene_active_vehicle_visibility']
        require(visibility[:5] == [1, 1, 0, 0, 0] and visibility[5:7] == [SELECTED, rows[0][7]],
                'Original UnHide9628 did not reveal independently owned selected APC once')
        result['collision'] = check_collision(guest['extra']['rf_scene_secondary_vehicle_collision_audit'])
        result['saved'] = saved_rows(payload, rows[158], rows[159], True)
        require(result['saved']['events'][str(RETIRE_TIMER)][4] == NONE and
            result['saved']['events'][str(REVIVE_TIMER)][4] not in (0, NONE),
            'Retirement save lost pending ordinary no-revival probe')
    else:
        require(all(row[12] == 1 and row[16:80] == previous['wire'] and row[80:114] == previous['full_rigid']
                    for row in rows.values()), 'Retired fresh owner moved or revived')
        initial_events, final_events = event_observations(rows[0]), event_observations(rows[118])
        # Zero-delay UnHide dispatch does not write the generic delayed-event
        # mode. Its actual source/actor, cooldown and exact visibility callback
        # below establish dispatch without inventing a mode transition.
        require(initial_events[REVIVE_TIMER][4] not in (0, NONE) and
            final_events[REVIVE_TIMER][4] == NONE and
            initial_events[REVIVE][5:7] == [0, 0] and final_events[REVIVE][5:7] == [NONE, NONE] and
            final_events[REVIVE][8] == initial_events[REVIVE][8] == 0 and
            final_events[REVIVE][9] != initial_events[REVIVE][9] and
            final_events[REVIVE][10:12] == [0, 0],
            'Saved timer did not exercise ordinary UnHide on the retired owner')
        require(not guest['extra']['rf_scene_secondary_vehicle'][9] and
            not any(guest['extra']['rf_scene_active_vehicle_visibility']),
            'Retired load replayed removal or selected reveal effects')
        visibility = guest['extra']['rf_scene_vehicle_visibility']
        require(visibility[0] == 1 and visibility[3] == 1 and visibility[4:6] == [SECONDARY, rows[0][5]],
                'UnHide did not take the normal retired-owner no-op path')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--run-root', type=Path)
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('Timeout must be positive')
    folder = args.prepare_only or args.run_root or ROOT / 'artifacts/xemu' / (
        'secondary-apc-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True, exist_ok=args.prepare_only is not None)
    level, recipe = prepare_level(folder / 'level')
    if args.prepare_only:
        print(json.dumps(dict(result='PREPARED_NOT_RUN', path=str(level), recipe=recipe), indent=2))
        return
    require_no_project_xemu(ROOT)
    hdd = prepare_hdd(ROOT, ROOT / 'local/xemu-harness/pacing-base.qcow2')
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', CONFIG}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe, phases={}, validation={}, binaries={}, input_sha256={},
        original_input_sha256={name: sha(data) if data is not None else None for name, data in original.items()},
        limitations=LIMITATIONS, hdd=str(hdd), source_and_retirement_persist_test_hdd=True)
    restore = folder / 'input-restore'
    restore.mkdir()
    for name, data in original.items():
        if data is not None:
            (restore / name).write_bytes(data)
    (restore / 'manifest.json').write_text(json.dumps(report['original_input_sha256'], indent=2) + '\n')

    def write_report():
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    try:
        saved = None
        for phase, label, frames in ((1, 'source', SOURCE_FRAMES), (2, 'retire', RETIRE_FRAMES), (3, 'retired', RETIRED_FRAMES)):
            inputs = phase_inputs(level, phase)
            for name in names:
                (DISC / name).unlink(missing_ok=True)
            for name, data in inputs.items():
                require(name in names, 'Untracked disc input ' + name)
                (DISC / name).write_bytes(data)
            report['input_sha256'][label] = {name: sha(data) for name, data in inputs.items()}
            write_report()
            build(folder, label)
            report['binaries'][label] = preserve_launch_binaries(folder / (label + '-binaries'))
            write_report()
            guest = run_guest(folder, label, hdd, frames, args.seconds, snapshot=phase == 3,
                capture_world=phase != 3, extra_symbols=SYMBOLS, allow_guest_error=True)
            report['phases'][label] = guest
            write_report()
            require(guest['guest_phase'] == 5 and guest['frames'] == frames,
                f"{label}: incomplete Xbox phase={guest['guest_phase']:#x} frames={guest['frames']}/{frames} "
                f"load_stage={guest['campaign_load_stage']} checkpoint={guest['checkpoint_state']}")
            payload = (folder / label / 'xbox-world.rfwc').read_bytes() if phase != 3 else None
            if phase == 1:
                report['validation'][label] = validate_source(guest, payload, recipe)
            else:
                report['validation'][label] = validate_loaded(guest, saved, recipe, phase, payload)
            if phase != 3:
                saved = report['validation'][label]['saved']
            write_report()
        report['result'] = 'PASS'
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        for name, data in original.items():
            (DISC / name).unlink(missing_ok=True)
            if data is not None:
                (DISC / name).write_bytes(data)
        try:
            build(folder, 'restore')
        except Exception as exc:
            report['result'] = 'FAIL'
            report['restore_build_error'] = str(exc)
            raise
        finally:
            report['restored_input_sha256'] = {name: sha((DISC / name).read_bytes()) if (DISC / name).exists() else None for name in sorted(names)}
            report['disc_restored'] = report['original_input_sha256'] == report['restored_input_sha256']
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            write_report()
            print(folder, report['result'], flush=True)
        require(report['disc_restored'], 'Original disc inputs were not restored exactly')


if __name__ == '__main__':
    main()
