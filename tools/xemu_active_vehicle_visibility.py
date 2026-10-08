"""Bounded Xbox selected-host APC visibility and ordinary save/fresh-load.

Original L1S3 APC9627 stays authored hidden with health5000. The established
enemy-free CTF06 fixture edits its transform only. Byte-exact UnHide9628
reveals it; the second case uses ordinary Invert to send that event OFF.
Process-contained replay proves hidden Use rejection and visible boarding.
Native read-only queries inspect exact current hull/registry identity. No
images, host input, guest memory writes, PC runtime, or campaign routes.

--prepare-only writes disposable fixtures and replay metadata. Only the parent
operator may invoke the runner; builds, emulator ownership and cleanup remain
the parent's responsibility. The runner never invokes generated cleanup.
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
from check_hit_event import event
from inspect_levels import inspect
from xemu_apc_moving_exit import prepare_level as prepare_apc
from xemu_npc_turret_seat import record_details
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import event_rows, section_payload
from xemu_vehicle_visibility import command, creation_offsets
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd

HOST, SHOW, HIDDEN, NONE = 9627, 9628, 0x4000, 0xffffffff
START_VISIBLE, START_HIDDEN, SHOW_TIMER, HIDE_TIMER, HIDE = range(916500, 916505)
SOURCE_FRAMES, LOAD_FRAMES = 300, 120
SAMPLE_FRAMES = (0, 20, 40, 100, 118, 150, 200, 250, 298)
OBSERVATION = 'rf_scene_active_vehicle_visibility_samples'
SYMBOLS = {OBSERVATION: len(SAMPLE_FRAMES) * 56,
    'rf_scene_active_vehicle_visibility_fixture': 8,
    'rf_scene_active_vehicle_visibility_audit': 24,
    'rf_scene_active_vehicle_seat_clearance': 32,
    'rf_scene_active_vehicle_visibility': 8, 'rf_scene_active_vehicle_visibility_history': 64,
    'rf_scene_active_vehicle_draw': 4, 'rf_scene_vehicle_state': 16,
    'rf_scene_vehicle_entry_probe': 16, 'rf_scene_vehicle_damage': 8,
    'rf_scene_vehicle_physics_checkpoint': 8, 'rf_scene_vehicle_physics_restore': 24,
    'rf_scene_setup_result': 4, 'rf_scene_vehicle_route_state': 8,
    'rf_scene_enemy_combat': 8, 'rf_scene_apc_primary': 8,
    'rf_scene_world_load_reject': 3, 'rf_scene_checkpoint_world_reject': 9}
LIMITATIONS = ('One original selected APC in an enemy-free geometry fixture; '
    'visible occupied and hidden unoccupied ordinary save/fresh-load. '
    'No occupied Hide, vehicle motion, NPC passengers, death/resurrection, '
    'guided weapon, visual/audio appearance or campaign claim.')


def replay(case, loading=False):
    if case not in ('visible', 'hidden'):
        raise ValueError('Unknown visibility case')
    frames = LOAD_FRAMES if loading else SOURCE_FRAMES
    uses = (30, 80) if loading else (30, 180) if case == 'visible' else (30, 240)
    return b'RFI6' + U(48) + b''.join(struct.pack('<5f7I',
        0, 0, 0, 0, 0, 0, 0, int(frame in uses), 0, 0, 0, 0)
        for frame in range(frames))


def prepare_level(folder):
    path, recipe = prepare_apc(folder)
    for old in ('frames', 'schedule', 'limits'):
        recipe.pop(old, None)
    # The reused preparer supplies a moving-exit replay. It is not an input
    # for this fixture; retain only the explicitly named visibility replays.
    (folder / 'player-replay.bin').unlink()
    original = read_entry(ROOT / 'Installed_Game/levels1.vpp', 'L1S3.rfl')
    original_meta = inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='L1S3.rfl'))
    source_host = next(row for row in entity_rows(original) if row['uid'] == HOST)
    details = record_details(source_host)
    _, hidden_at = creation_offsets(source_host)
    if source_host['name'] != 'APC' or source_host['raw'][hidden_at] != 1 or \
            details != dict(seat_host_uid=-1, authored_health=5000., authored_armor=0.):
        raise RuntimeError('Original living authored-hidden APC metadata changed')
    source_show = next(row for row in event_rows(section_payload(original, original_meta, 0x600)) if row['uid'] == SHOW)
    if source_show['type'] != 'UnHide' or source_show['links'] != [HOST]:
        raise RuntimeError('Original APC UnHide9628 target changed')
    staged = read_entry(path, 'L1S3.rfl')
    meta = inspect(io.BytesIO(staged), dict(offset=0, size=len(staged), name='L1S3.rfl'))
    host = entity_rows(staged)[0]
    at = host['transform']
    if host['raw'][:at] != source_host['raw'][:at] or host['raw'][at + 48:] != source_host['raw'][at + 48:]:
        raise RuntimeError('Staged APC changed a non-transform byte')
    records = [source_show['raw'],
        event(START_VISIBLE, 'Delay', 'start_visible_apc_case', (SHOW_TIMER,)),
        event(START_HIDDEN, 'Delay', 'start_hidden_apc_case', (SHOW_TIMER, HIDE_TIMER)),
        command(SHOW_TIMER, 'Delay', 'reveal_authored_apc', (SHOW,), 2),
        command(HIDE_TIMER, 'Delay', 'hide_revealed_apc', (HIDE,), 3.5),
        event(HIDE, 'Invert', 'ordinary_off_original_unhide', (SHOW,))]
    events = U(len(records)) + b''.join(records)
    out = bytearray(staged[:meta['sections'][0]['offset']])
    offsets = {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        data = events if kind == 0x600 else staged[section['offset'] + 8:section['offset'] + 8 + section['size']]
        offsets[kind] = len(out)
        out += U(kind, len(data)) + data
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    checked_meta = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L1S3.rfl'))
    checked = event_rows(section_payload(out, checked_meta, 0x600))
    if [row['raw'] for row in checked] != records or entity_rows(out)[0]['raw'] != host['raw']:
        raise RuntimeError('Independent staged event/entity roundtrip failed')
    # Every non-event section remains the established original geometry fixture.
    for section in meta['sections']:
        kind = int(section['type'], 16)
        if kind != 0x600 and section_payload(staged, meta, kind) != section_payload(out, checked_meta, kind):
            raise RuntimeError('Visibility fixture changed an undeclared section')
    archive(path, [('L1S3.rfl', out)])
    recipe.update(scope=__doc__, details=details, original_hidden_byte=1,
        original_show_sha256=sha(source_show['raw']), original_show_hex=source_show['raw'].hex(),
        original_host_sha256=sha(source_host['raw']), staged_host_sha256=sha(host['raw']),
        fixture_sha256=sha(bytes(out)), archive_sha256=sha(path.read_bytes()),
        events=[dict(uid=row['uid'], type=row['type'], links=row['links'], sha256=sha(row['raw'])) for row in checked],
        source_frames=SOURCE_FRAMES, load_frames=LOAD_FRAMES,
        staged_fields=['established APC transform/player orientation', 'isolated ordinary event graph',
                       'empty unrelated events/triggers; byte-exact original UnHide9628 retained'],
        source_timeline=dict(hidden_Use=30, setup=0, original_UnHide=120,
            visible_case_board=180, hidden_case_Invert_OFF=210, hidden_case_Use=240, ordinary_save=300),
        load_timeline=dict(restored_observation=0, exit_or_hidden_Use=30, reboard_or_hidden_Use=80, end=120),
        observation_frames=list(SAMPLE_FRAMES), limitations=LIMITATIONS)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    for case in ('visible', 'hidden'):
        (folder / (case + '-source-replay.bin')).write_bytes(replay(case))
        (folder / (case + '-load-replay.bin')).write_bytes(replay(case, True))
    return path, recipe


def observed(guest, frame):
    at = SAMPLE_FRAMES.index(frame) * 56
    row = guest['extra'][OBSERVATION][at:at + 56]
    if len(row) != 56 or row[:3] != [1, frame, HOST] or row[4:6] != [1, 1] or \
            row[3] in (0, NONE) or not row[3] >> 16 or row[14] in (0, NONE, row[3]) or \
            f(row[6]) != 5000. or f(row[7]) != 0. or row[53:55] != [0, 1] or \
            row[26] or row[34] or not 1 <= row[33] <= 8 or not math.isfinite(f(row[41])) or f(row[41]) <= 0:
        raise RuntimeError('Invalid exact living APC observation at frame%d: %r' % (frame, row))
    if bool(row[8] & HIDDEN) != bool(row[9] & HIDDEN) or (row[8] | row[9]) & 2:
        raise RuntimeError('Damage/view visibility flags disagree or owner is dead')
    return row


def decoded(row):
    return dict(frame=row[1], uid=row[2], host_handle=row[3], registry_exact=bool(row[4]),
        entity_registry_exact=bool(row[5]), health=f(row[6]), flags=row[8], view_flags=row[9],
        occupied=row[10], session_host=row[11], session_driver=row[12], host_driver=row[13],
        player_handle=row[14], player_link=row[15], control_host=row[16], control=row[17],
        entry_calls=row[18], entry_allowed=row[19], entries=row[20], exits=row[21],
        draw=dict(frame=row[22], submissions=row[23], vertices=row[24], hidden=row[25]),
        projectile=dict(status=row[26], matched=row[27], tag=row[28], handle=row[29], fraction=f(row[30])),
        target_eligible=row[31], player_hull=dict(status=row[34], matched=row[35], handle=row[36], fraction=f(row[37])),
        sphere_center=[f(value) for value in row[38:41]], sphere_radius=f(row[41]),
        host_position=[f(value) for value in row[42:45]], player_position=[f(value) for value in row[45:48]],
        last_entry_frame=row[48], last_entry_stage=row[49], event_calls=row[51])


def check_state(row, hidden, occupied, draw=True):
    if bool(row[8] & HIDDEN) != hidden or row[10] != occupied or row[31] != int(not hidden):
        raise RuntimeError('Visibility/occupancy/real target admission mismatch: %r' % decoded(row))
    if occupied:
        if row[11:14] != [row[3], row[14], row[14]] or row[15:18] != [row[3]] * 3:
            raise RuntimeError('Exact occupied player/host ownership mismatch')
    elif row[13] != NONE or row[15:18] != [NONE, NONE, row[14]]:
        raise RuntimeError('Unoccupied APC retained a player control link')
    if hidden:
        if row[27] or row[35]:
            raise RuntimeError('Hidden host admitted an actual projectile/player hull contact')
    else:
        if row[27:30] != [1, 0x08000000, row[3]] or not 0 <= f(row[30]) < 1:
            raise RuntimeError('Visible actual projectile query missed exact host handle/tag')
        if not occupied and (row[35:37] != [1, row[3]] or not 0 <= f(row[37]) < 1):
            raise RuntimeError('Visible actual player-hull query missed exact host')
        if occupied and row[35]:
            raise RuntimeError('Occupied player query lost its own-host exclusion')
    if draw:
        if row[22] != row[1] or row[25] != int(hidden):
            raise RuntimeError('Draw observation does not describe this frame/visibility')
        if hidden and (row[23] or row[24]):
            raise RuntimeError('Hidden APC submitted mesh geometry')
        if not hidden and not occupied and (not row[23] or not row[24]):
            raise RuntimeError('Visible unoccupied APC did not submit mesh geometry')


def check_guest(guest, frames, loading=False, payload_bytes=None):
    if guest['guest_phase'] != 5 or guest['frames'] != frames or guest['memory_bytes'] != 64 * 1024 * 1024 or \
            guest['free_pages'] <= 0 or guest['player_life'][2]:
        raise RuntimeError('Incomplete living bounded stock64MiB guest')
    x = guest['extra']
    fixture = x['rf_scene_active_vehicle_visibility_fixture']
    if fixture[:2] != [1, 2 if loading else 1] or fixture[3] != (5 if loading else 9) or \
            fixture[4] != HOST or fixture[6:] != [1, 0]:
        raise RuntimeError('Missing explicit native visibility observation opt-in: %r' % fixture)
    if x['rf_scene_vehicle_state'][5] or x['rf_scene_enemy_combat'][2] or x['rf_scene_enemy_combat'][7] or \
            x['rf_scene_vehicle_route_state'][3] or x['rf_scene_vehicle_route_state'][7] or \
            x['rf_scene_apc_primary'][1] or x['rf_scene_vehicle_damage'][2] or x['rf_scene_vehicle_damage'][3]:
        raise RuntimeError('Unexpected vehicle error, combat, route or damage')
    state = guest['checkpoint_state']
    if loading and (state[8] != 1 or state[0] or state[1] != payload_bytes or state[9] or
            any(x['rf_scene_setup_result']) or any(x['rf_scene_active_vehicle_visibility']) or
            any(x['rf_scene_active_vehicle_visibility_history']) or any(x['rf_scene_world_load_reject'])):
        raise RuntimeError('Fresh load failed, saved unexpectedly or replayed setup/visibility callbacks')


def validate_source(guest, case):
    check_guest(guest, SOURCE_FRAMES)
    audit = guest['extra']['rf_scene_active_vehicle_visibility_audit']
    if audit[0:3] != [1, 150, HOST] or audit[4] != audit[5] or audit[6:9] != [128, 216, 2] or \
            audit[13] != 1 or audit[22:24] != [0x3fff, 0] or \
            any(audit[index] for index in (10, 11, 12, 14, 15, 18, 19, 20)):
        raise RuntimeError('Native visibility codec audit failed: %r' % audit)
    if guest['extra']['rf_scene_active_vehicle_visibility_fixture'][2] != int(case == 'hidden'):
        raise RuntimeError('Source observation case differs from the requested replay')
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES}
    clearance = guest['extra']['rf_scene_active_vehicle_seat_clearance']
    if clearance[:10] != [1, 150, rows[150][14], rows[150][3], 0, 0, 0, 0, 1, 15] or \
            clearance[10] >= 64 or not math.isfinite(f(clearance[11])) or f(clearance[11]) <= 0:
        raise RuntimeError('Actual-head negative/positive seated clearance failed: %r' % clearance)
    floor = clearance[12:]
    if floor[:3] != [1, 1, 0] or floor[4] != 171 or floor[5] != 0 or floor[6] != clearance[11] or \
            floor[7] != floor[10] or floor[9] != floor[12] or \
            abs(f(floor[8]) - f(floor[11]) - 8.) > .00001 or \
            not 0 < f(floor[13]) < 1 or f(floor[15]) <= .5 or any(floor[17:]):
        raise RuntimeError('Actual-head downward sweep did not record the exact central-floor hit: %r' % floor)
    if not all(math.isfinite(f(value)) for value in floor[6:17]):
        raise RuntimeError('Nonfinite actual-head clearance query evidence')
    if audit[3] != rows[150][3] or any(audit[index] != 0xfffffffe for index in (16, 17, 21)):
        raise RuntimeError('Native codec audit owner or rejection statuses mismatch')
    identity = {(row[3], row[14]) for row in rows.values()}
    if len(identity) != 1:
        raise RuntimeError('Source APC/player full handles changed')
    for frame in (0, 20, 40, 100, 118):
        check_state(rows[frame], True, 0)
    if rows[40][18:22] != [1, 0, 0, 0] or rows[40][48] != 30 or rows[40][55]:
        raise RuntimeError('Actual hidden Use30 did not reach and fail ordinary admission')
    check_state(rows[150], False, 0)
    if rows[150][8] ^ rows[100][8] != HIDDEN:
        raise RuntimeError('Reveal changed unrelated object flags')
    for frame in (200, 250, 298):
        hidden = case == 'hidden' and frame >= 250
        check_state(rows[frame], hidden, int(case == 'visible'))
    end = rows[298]
    expected = [2, 1, 1, 0] if case == 'visible' else [2, 0, 0, 0]
    if end[18:22] != expected or end[48] != (180 if case == 'visible' else 240) or end[55]:
        raise RuntimeError('Source ordinary Use admission/boarding count mismatch: %r' % decoded(end))
    x = guest['extra']
    setup = START_VISIBLE if case == 'visible' else START_HIDDEN
    if x['rf_scene_setup_result'] != [1, setup, 48, 0]:
        raise RuntimeError('Unexpected isolated setup result')
    calls = 1 if case == 'visible' else 2
    expected_counter = [calls, 1, calls - 1, 0, 0, HOST, end[3], end[8]]
    if x['rf_scene_active_vehicle_visibility'] != expected_counter:
        raise RuntimeError('Original UnHide/OFF callback history mismatch')
    history = x['rf_scene_active_vehicle_visibility_history']
    expected_history = [120, HOST, end[3], 1, rows[100][8], rows[150][8], rows[150][6], 0]
    if case == 'hidden':
        expected_history += [210, HOST, end[3], 0, rows[200][8], end[8], end[6], 0]
    if history[:calls * 8] != expected_history or any(history[calls * 8:]):
        raise RuntimeError('Visibility callbacks changed identity, health, timing or occupancy')
    return dict(result='PASS', observations={str(frame): decoded(row) for frame, row in rows.items()},
        seated_clearance=dict(player_handle=clearance[2], host_handle=clearance[3],
            actual_head_index=clearance[10], actual_head_radius=f(clearance[11]),
            floor_rejected=True, floor_solid=floor[3], floor_face=floor[4],
            head_start=[f(value) for value in floor[7:10]], head_end=[f(value) for value in floor[10:13]],
            fraction=f(floor[13]), normal=[f(value) for value in floor[14:17]],
            actual_player_to_authored_seat_clear=True, live_state_unchanged_mask=clearance[9]),
        callback_history=[history[i:i + 8] for i in range(0, calls * 8, 8)], free_pages=guest['free_pages'])


def validate_saved(guest, payload, case):
    state = guest['checkpoint_state']
    if state[9] != 1 or state[3] or state[4] != len(payload):
        raise RuntimeError('Ordinary world save failed')
    sections = checkpoint_sections(payload)
    wrapper = sections[11]
    if wrapper[:4] != b'RFPV' or len(wrapper) < 16:
        raise RuntimeError('Missing ordinary active visibility wrapper')
    version, size, count = struct.unpack_from('<3I', wrapper, 4)
    if (version, count) != (2, 1) or len(wrapper) != 16 + size + 72:
        raise RuntimeError('Expected exactly one RFPV2 active-owner row')
    physics = list(struct.unpack_from('<18I', wrapper, 16 + size))
    row = observed(guest, 298)
    if physics[:3] != [HOST, 0, 0] or physics[4] or physics[5] != row[8] & 0x06004000 or \
            bool(physics[5] & HIDDEN) != (case == 'hidden') or physics[17]:
        raise RuntimeError('RFPV2 did not retain exact selected-host visibility')
    if not all(math.isfinite(f(value)) for value in physics[8:17]):
        raise RuntimeError('RFPV2 contains nonfinite retained motion')
    vehicle = wrapper[16:16 + size]
    if len(vehicle) not in (128, 168) or vehicle[:4] != b'RFVC' or \
            struct.unpack_from('<2I', vehicle, 4) != (2, 128) or \
            struct.unpack_from('<2I', vehicle, 16) != (2, 3 if case == 'visible' else 1) or \
            struct.unpack_from('<2f', vehicle, 24) != (5000., 0.):
        raise RuntimeError('Unexpected original-profile APC save/vitals/occupancy')
    checksum = 2166136261
    for at, value in enumerate(vehicle[:128]):
        checksum = ((checksum ^ (0 if 12 <= at < 16 else value)) * 16777619) & 0xffffffff
    if checksum != struct.unpack_from('<I', vehicle, 12)[0]:
        raise RuntimeError('APC RFVC2 checksum mismatch')
    if len(vehicle) == 168 and (vehicle[128:132] != b'RFVR' or list(struct.unpack_from('<9I', vehicle, 132)) != [3] + [0] * 8):
        raise RuntimeError('Unexpected retained APC route/AI state')
    position = list(struct.unpack_from('<3I', vehicle, 32))
    if position != row[42:45]:
        raise RuntimeError('Saved host position differs from exact final physics observation')
    return dict(bytes=len(payload), sha256=sha(payload), physics=physics, position_bits=position,
        formats=['RFWC', 'RFPV2', 'RFVC2'], hidden=case == 'hidden', occupied=case == 'visible',
        health=5000., armor=0., ammo=list(struct.unpack_from('<2I', vehicle, 104)))


def validate_loaded(guest, saved, case):
    check_guest(guest, LOAD_FRAMES, True, saved['bytes'])
    if guest['extra']['rf_scene_active_vehicle_visibility_fixture'][2] != int(case == 'hidden'):
        raise RuntimeError('Load observation case differs from the requested replay')
    if any(guest['extra']['rf_scene_active_vehicle_seat_clearance']):
        raise RuntimeError('Fresh load reran the source-only seated-clearance observation')
    rows = {frame: observed(guest, frame) for frame in (0, 20, 40, 100, 118)}
    if len({(row[3], row[14]) for row in rows.values()}) != 1:
        raise RuntimeError('Fresh-load exact owner identity changed during continuation')
    for frame, row in rows.items():
        occupied = int(case == 'visible' and frame != 40)
        check_state(row, case == 'hidden', occupied, draw=frame != 0)
    if rows[0][42:45] != saved['position_bits']:
        raise RuntimeError('Fresh-load frame0 changed the saved host pose')
    if rows[20][20:22] != [0, 0]:
        raise RuntimeError('Fresh load replayed boarding/exit')
    if case == 'visible':
        if rows[40][20:22] != [0, 1] or rows[100][20:22] != [1, 1] or \
                rows[100][18:20] != [1, 1] or rows[100][48] != 80:
            raise RuntimeError('Restored originally-hidden APC did not exit30/reboard80 normally')
    elif rows[40][18:22] != [1, 0, 0, 0] or rows[40][48] != 30 or \
            rows[100][18:22] != [2, 0, 0, 0] or rows[100][48] != 80:
        raise RuntimeError('Restored hidden unoccupied host accepted or missed actual Use attempts')
    restore = guest['extra']['rf_scene_vehicle_physics_restore']
    physics = saved['physics']
    if restore[:5] != [1, HOST, physics[2], physics[3], physics[4]] or \
            restore[5] & 0x06004000 != physics[5] or restore[7:17] != physics[7:17] or restore[17]:
        raise RuntimeError('RFPV2 immutable restore trace differs from saved row')
    return dict(result='PASS', observations={str(frame): decoded(row) for frame, row in rows.items()},
        restored_visibility_without_callbacks=True, free_pages=guest['free_pages'])


def phase_inputs(level, case, loading=False):
    result = {'scene-fixture.vpp': level.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L1S3.rfl'.ljust(64, b'\0'),
        'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
        'campaign-active-visibility.bin': U(0x53495641, 2 if loading else 1, int(case == 'hidden'), 0),
        'player-replay.bin': replay(case, loading),
        'world-hdd-load.flag' if loading else 'world-hdd-save.flag': b'1'}
    if not loading:
        result['campaign-setup.bin'] = U(START_VISIBLE if case == 'visible' else START_HIDDEN)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--run-root', type=Path)
    parser.add_argument('--cases', nargs='+', choices=('visible', 'hidden'), default=['visible', 'hidden'])
    parser.add_argument('--seconds', type=int, default=480)
    args = parser.parse_args()
    if args.seconds <= 0 or len(args.cases) != len(set(args.cases)):
        parser.error('Positive timeout and unique cases are required')
    folder = args.prepare_only or args.run_root or ROOT / 'artifacts/xemu' / (
        'active-vehicle-visibility-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True, exist_ok=args.prepare_only is not None)
    level, recipe = prepare_level(folder / 'level')
    if args.prepare_only:
        print(json.dumps(dict(result='PASS', status='PREPARED_NOT_RUN', folder=str(folder),
            archive_sha256=recipe['archive_sha256'], original_show_sha256=recipe['original_show_sha256'],
            frames=dict(source=SOURCE_FRAMES, load=LOAD_FRAMES), limitations=LIMITATIONS), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated test HDD base')
    hdd = prepare_hdd(ROOT, base)
    names = set(FLAGS) | {path.name for path in DISC.glob('campaign-*') if path.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', 'campaign-active-visibility.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe, limitations=LIMITATIONS,
        cases={}, binaries={}, input_sha256={}, original_input_sha256={name: sha(data) if data is not None else None
        for name, data in original.items()})
    restore = folder / 'input-restore'
    restore.mkdir()
    for name, data in original.items():
        if data is not None:
            (restore / name).write_bytes(data)
    (restore / 'manifest.json').write_text(json.dumps(report['original_input_sha256'], indent=2) + '\n')

    def write_report():
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    def stage(label, inputs):
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        for name, data in inputs.items():
            if name not in names:
                raise RuntimeError('Untracked fixture input ' + name)
            (DISC / name).write_bytes(data)
        report['input_sha256'][label] = {name: sha(data) for name, data in inputs.items()}
        write_report()
        build(folder, label)
        report['binaries'][label] = preserve_launch_binaries(folder / (label + '-binaries'))
        write_report()

    try:
        for case in args.cases:
            result = report['cases'][case] = {}
            label = case + '-source'
            stage(label, phase_inputs(level, case))
            source = run_guest(folder, label, hdd, SOURCE_FRAMES, args.seconds,
                capture_world=True, extra_symbols=SYMBOLS, allow_guest_error=True)
            result['source'] = source
            write_report()
            result['source_validation'] = validate_source(source, case)
            payload = (folder / label / 'xbox-world.rfwc').read_bytes()
            saved = result['saved'] = validate_saved(source, payload, case)
            label = case + '-load'
            stage(label, phase_inputs(level, case, True))
            loaded = run_guest(folder, label, hdd, LOAD_FRAMES, args.seconds,
                snapshot=True, extra_symbols=SYMBOLS, allow_guest_error=True)
            result['loaded'] = loaded
            write_report()
            result['load_validation'] = validate_loaded(loaded, saved, case)
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
            report['restored_input_sha256'] = {name: sha((DISC / name).read_bytes())
                if (DISC / name).exists() else None for name in sorted(names)}
            report['disc_restored'] = report['original_input_sha256'] == report['restored_input_sha256']
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            write_report()
            print(folder, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Original staged disc inputs were not restored')


if __name__ == '__main__':
    main()
