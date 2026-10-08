"""Prepare/run a bounded stock64MiB independent Fighter live-player pursuit.

Keep original L20S2 Fighter18155, UnHide18157 and Goto_Player18156 byte-for-byte,
including original geometry and player start. Isolate other actors/controllers/
triggers. Ordinary Delay starts the chain; a saved Delay/Invert later sends OFF.
RFI6 alone moves the player. Two120-frame boots cover live steering, an ordinary
RFSV2 save/fresh load without setup replay, and OFF with ordinary drag/coasting.
No images, host input, campaign routes, autonomous weapons or avoidance claims.

Exactly one of --prepare-only or --run-root is required. Parent exclusively owns
builds/XEMU/cleanup. Preparation only writes a disposable fixture and source
evidence, reading Installed_Game without modifying it.
"""
import argparse
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import actor_details, event_rows, section_payload
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd
import xemu_secondary_apc as apc

OWNER, SHOW, GOTO = 18155, 18157, 18156
START, STOP_TIMER, STOP = 916900, 916901, 916902
EVENTS = (SHOW, GOTO, START, STOP_TIMER, STOP)
NONE, FRAMES, ROW_WORDS = 0xffffffff, 120, 224
SAMPLE_FRAMES = (0, 20, 40, 58, 60, 78, 100, 118, 119)
CONFIG = 'campaign-fighter-pursuit.bin'
OBSERVATION = 'rf_scene_secondary_fighter_pursuit_samples'
HISTORY = 'rf_scene_secondary_fighter_pursuit_history'
SYMBOLS = {key: value for key, value in apc.SYMBOLS.items()
    if key not in (apc.OBSERVATION, 'rf_scene_secondary_vehicle_samples',
        'rf_scene_secondary_vehicle_fixture', 'rf_scene_secondary_vehicle_clocks',
        'rf_scene_secondary_vehicle_collision_audit')}
SYMBOLS.update({OBSERVATION: len(SAMPLE_FRAMES)*ROW_WORDS, HISTORY: FRAMES*48,
    'rf_scene_secondary_fighter_pursuit_fixture': 8,
    'rf_scene_secondary_fighter_pursuit_clocks': len(SAMPLE_FRAMES),
    'rf_scene_secondary_fighter_pursuit_order_audit': 16,
    'rf_scene_secondary_fighter_pursuit_gate_audit': 16,
    'rf_scene_secondary_vehicle_pursuit': 32, 'rf_scene_secondary_vehicle_pool_restore': 160,
    'rf_scene_world_controller_steps': 8})
LIMITS = ('One original unoccupied Fighter on a short unobstructed first leg, if admitted by '
    'actual collision. No avoidance, complete route, all-four encounter, riders, weapons or campaign claim. '
    'Pure arrival/gate tests supplement the live-player/save/OFF check; they do not claim natural arrival or live riders.')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def replay(phase):
    # move[0] is process-contained right/left input. No aim/fire/use or host input.
    return b'RFI6' + U(48) + b''.join(struct.pack('<5f7I',
        float(1 if phase == 1 and 20 <= frame < 78 else
            -1 if phase == 2 and 5 <= frame < 40 else 0),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) for frame in range(FRAMES))


def prepare_level(folder, source_root=ROOT):
    folder.mkdir(parents=True, exist_ok=True)
    source = read_entry(source_root / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L20S2.rfl'))
    require(meta['version'] == 180 and not meta['trailing_bytes'], 'Incomplete original L20S2')
    original_owner = next(row for row in entity_rows(source) if row['uid'] == OWNER)
    details = actor_details(original_owner)
    require(original_owner['name'] == 'Fighter01' and details['health'] == 900 and
        details['armor'] == 0 and details['creation_flags'] == 2 and details['seat_host_uid'] == -1,
        'Original hidden/unoccupied Fighter changed')
    original = {row['uid']: row for row in event_rows(section_payload(source, meta, 0x600))}
    require(original[SHOW]['type'] == 'UnHide' and original[SHOW]['links'] == [OWNER, GOTO] and
        original[GOTO]['type'] == 'Goto_Player' and original[GOTO]['links'] == [OWNER], 'Original chain changed')
    authored_point = list(struct.unpack_from('<3f', original[GOTO]['raw'], 6 + len(original[GOTO]['type'])))
    events = [original[uid]['raw'] for uid in (SHOW, GOTO)] + [
        command(START, 'Delay', 'start_original_player_pursuit', (SHOW, STOP_TIMER)),
        command(STOP_TIMER, 'Delay', 'stop_saved_player_pursuit', (STOP,), delay=3.),
        event(STOP, 'Invert', 'ordinary_off_to_original_goto_player', (GOTO,))]
    replacements = {0x30000: U(1) + original_owner['raw'], 0x600: U(len(events)) + b''.join(events),
        0x3000: U(0), 0x40000: U(0), 0x50000: U(0), 0x60000: U(0)}
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
    checked = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='L20S2.rfl'))
    require(checked['player_offset_matches'] and checked['info_offset_matches'] and not checked['trailing_bytes'],
        'Fixture section roundtrip failed')
    require([row['raw'] for row in entity_rows(out)] == [original_owner['raw']], 'Original Fighter bytes changed')
    decoded = event_rows(section_payload(out, checked, 0x600))
    require([row['raw'] for row in decoded] == [bytes(row) for row in events] and
        tuple(row['uid'] for row in decoded) == EVENTS, 'Fixture event roundtrip failed')
    for kind, digest in preserved.items():
        require(sha(section_payload(out, checked, int(kind, 16))) == digest, 'Preserved section changed: ' + kind)
    path = folder / 'scene-fixture.vpp'
    archive(path, [('L20S2.rfl', out)])
    require(read_entry(path, 'L20S2.rfl') == out, 'Archive roundtrip failed')
    recipe = dict(status='PREPARED_NOT_RUN', source='levels2.vpp/L20S2.rfl', source_sha256=sha(source),
        fixture_sha256=sha(out), archive_sha256=sha(path.read_bytes()), preserved_sections=preserved,
        owner=dict(uid=OWNER, details=details, sha256=sha(original_owner['raw']),
            transform=list(struct.unpack_from('<12f', original_owner['raw'], original_owner['transform']))),
        authored_point=authored_point, events=[dict(uid=row['uid'], type=row['type'], name=row['name'],
            links=row['links'], sha256=sha(row['raw'])) for row in decoded],
        original_event_sha256={str(uid): sha(original[uid]['raw']) for uid in (SHOW, GOTO)},
        observation_frames=SAMPLE_FRAMES, frames=FRAMES, stop_seconds=3., limitations=LIMITS)
    for phase, label in ((1, 'source'), (2, 'loaded')):
        (folder / (label + '-replay.bin')).write_bytes(replay(phase))
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    return path, recipe


def phase_inputs(level, phase):
    inputs = {'scene-fixture.vpp': level.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'),
        'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
        CONFIG: U(0x4c504653, phase, 0, 0), 'player-replay.bin': replay(phase)}
    if phase == 1:
        inputs.update({'campaign-setup.bin': U(START), 'world-hdd-save.flag': b'1'})
    else:
        inputs['world-hdd-load.flag'] = b'1'
    return inputs


def observed(guest, frame):
    at = SAMPLE_FRAMES.index(frame)*ROW_WORDS
    row = guest['extra'][OBSERVATION][at:at+ROW_WORDS]
    require(len(row) == ROW_WORDS and row[:2] == [1, frame] and not row[3], 'Missing/error native sample ' + str(frame))
    return row


def history(guest):
    data = guest['extra'][HISTORY]
    require(len(data) == FRAMES*48, 'Incomplete command/pose history')
    return [data[at:at+48] for at in range(0, len(data), 48)]


def vectors(words):
    return [f(value) for value in words]


def check_identity(row, recipe):
    wire = row[16:80]
    require(row[4] == OWNER and row[6:8] == [0, 5] and row[9:12] == [NONE, 1, 5] and not any(row[13:16]) and
        row[117] == 2 and row[212:222] == [1, 1, 11, 2, NONE, NONE, NONE, 0, 0, 0] and row[223] == 1,
        'Independent Fighter ownership/publication/seats changed')
    require(row[5] not in (0, NONE) and row[8] not in (0, NONE) and row[5] != row[8], 'Missing/distinct full handles')
    require(wire[:3] == [OWNER, 5, 0] and wire[7:11] == [GOTO, 1, 0, 0] and
        wire[36:39] == list(struct.unpack('<3I', struct.pack('<3f', *recipe['authored_point']))) and
        not any(wire[39:50]) and wire[12:30] == row[80:98] and wire[30:36] == row[107:113] and
        f(wire[50]) == 900 and f(wire[51]) == 0 and not wire[52] & (2 | 0x4000) and
        not wire[55] and (row[222] & (2 | 0x4000)) == (wire[52] & (2 | 0x4000)),
        'Wire/rigid/visibility/vitals or authored point changed')
    require(row[164:167] == [GOTO, row[165], 6], 'Original Goto_Player type changed')


def steer(position, basis, target):
    dx, dy, dz = [target[i]-position[i] for i in range(3)]
    if math.sqrt(dx*dx+dy*dy+dz*dz) <= 3:
        return [0., 0., 0.]
    angle = math.atan2(basis[8]*dx-basis[6]*dz, basis[6]*dx+basis[8]*dz)
    yaw = max(-1., min(1., angle/.7))
    throttle = 0. if abs(angle) > 1.5 else .2 if abs(angle) > .8 else min(1., math.hypot(dx, dz)/8.)
    return [throttle, max(-1., min(1., dy/6.)), yaw]


def check_motion(guest, recipe, phase):
    rows = history(guest)
    commands, responsive, distinct_point, actual_turns = 0, 0, 0, 0
    anchor = vectors(rows[0][12:15])
    for frame in range(1, FRAMES-1):
        row, previous = rows[frame], rows[frame-1]
        require(row[47] == previous[47]+1, 'Independent owner did not step exactly once')
        p = row[15:47]
        if p[0] == previous[15]:
            require(phase == 2 and frame >= 58, 'Pursuit unexpectedly stopped before ordinary OFF')
            continue
        require(p[0] == previous[15]+1 and p[1] == OWNER and p[5:8] == [1, 2, 1] and
            p[11:14] == previous[12:15] and p[20:32] == previous[:12], 'Actual command lost current player/start pose')
        expected = steer(vectors(p[20:23]), vectors(p[23:32]), vectors(p[11:14]))
        actual = [f(p[14]), f(p[16]), f(p[17])]
        require(max(abs(a-b) for a, b in zip(actual, expected)) < 2e-5 and p[19] == 1,
            'Actual command differs from independently computed live-player steering')
        stale = steer(vectors(p[20:23]), vectors(p[23:32]), anchor)
        fixed = steer(vectors(p[20:23]), vectors(p[23:32]), recipe['authored_point'])
        responsive += max(abs(a-b) for a, b in zip(actual, stale)) > 1e-4
        distinct_point += max(abs(a-b) for a, b in zip(actual, fixed)) > .01
        old_basis, new_basis = vectors(previous[3:12]), vectors(row[3:12])
        turn = math.atan2(old_basis[8]*new_basis[6]-old_basis[6]*new_basis[8],
            old_basis[6]*new_basis[6]+old_basis[8]*new_basis[8])
        actual_turns += actual[2]*turn > 1e-7
        commands += 1
    require(rows[-1] == rows[-2], 'Presentation-only final frame advanced world state')
    player_movement = max(math.dist(vectors(row[12:15]), anchor) for row in rows)
    owner_movement = math.dist(vectors(rows[20][:3]), vectors(rows[40][:3]))
    require(commands > 20 and responsive > 10 and distinct_point > 10 and actual_turns > 3 and
        player_movement > 1. and owner_movement > .1,
        'Missing actual player motion, responsive steering or Fighter displacement')
    return dict(actual_commands=commands, player_motion_response_frames=responsive,
        distinct_from_fixed_Goto_frames=distinct_point, actual_turn_response_frames=actual_turns, player_displacement=player_movement,
        fighter_displacement_frames20_40=owner_movement)


def healthy(guest, phase):
    x, state = guest['extra'], guest['checkpoint_state']
    require(guest['guest_phase'] == 5 and guest['frames'] == FRAMES and
        guest['memory_bytes'] == 64*1024*1024 and guest['free_pages'] > 0 and not guest['player_life'][2],
        'Incomplete living stock64MiB phase')
    require(x['rf_scene_secondary_fighter_pursuit_fixture'] == [1, phase, 9, OWNER, GOTO, 1, int(phase == 1), 0] and
        not x['rf_scene_secondary_vehicle'][13] and not any(guest['level_transitions']) and
        not any(guest['level_request']) and not any(x['rf_scene_world_load_reject']), 'Observer/runtime/level error')
    world = x['rf_scene_world_controller_steps']
    require(world[0] == world[1] and not world[6] and not world[7], 'Duplicate/error world step')
    if phase == 1:
        require(x['rf_scene_setup_result'] == [1, START, 48, 0] and state[9] == 1 and not state[3], 'Ordinary chain/save failed')
        for name, count in (('order', 13), ('gate', 10)):
            require(x['rf_scene_secondary_fighter_pursuit_'+name+'_audit'][:4] == [count, count, 0, 0], 'Native '+name+' checks failed')
        audit = x['rf_scene_secondary_vehicle_checkpoint_audit']
        require(audit[0] == 1 and audit[1] >= 18 and not audit[3] and not audit[4] and audit[7] == 1,
            'Pursuit checkpoint rejection/atomicity audit failed')
    else:
        require(state[8] == 1 and not state[0] and not state[9] and not any(x['rf_scene_setup_result']),
            'Fresh restore failed, saved again or replayed setup')


def saved_state(payload, row):
    sections = checkpoint_sections(payload)
    data = sections[11]
    if data[:4] == b'RFPV':
        inner = struct.unpack_from('<I', data, 8)[0]
        data = data[16:16+inner]
    require(data[:4] == b'RFSV', 'Missing RFSV2')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require(version == 2 and count == 1 and len(data) == 16+inner+256, 'RFSV2 layout changed')
    wire = list(struct.unpack_from('<64I', data, 16+inner))
    require(wire == row[16:80] and wire[6] == 1 and not any(wire[59:]), 'Save differs from exact active pursuit')
    passive = data[16:16+inner]
    require(passive[:4] == b'RFVA' and struct.unpack_from('<3I', passive, 4) == (2, 0, 1) and len(passive) == 96,
        'RFVA2 lost independent owner or invented a selected host')
    require(list(struct.unpack_from('<12I', passive, 24)) == wire[12:24], 'RFVA saved pose differs')
    native = {row[at]: row[at:at+12] for at in range(152, 212, 12)}
    saved_events = {}
    require(len(sections[4]) == len(EVENTS)*192, 'Saved event count changed')
    for at in range(0, len(sections[4]), 192):
        raw = sections[4][at:at+192]
        require(raw[:4] == b'RFEC' and struct.unpack_from('<2I', raw, 4) == (3, 192), 'Expected RFEC3')
        apc.check_checksum(raw, 'RFEC3')
        words = list(struct.unpack('<48I', raw)); uid = words[4]
        require(uid in native and str(uid) not in saved_events, 'Unexpected saved event identity')
        values = native[uid].copy()
        require(values[2] == words[5] and values[7:9] == words[16:18], 'Saved event state changed')
        values[4] = words[40]
        if values[2] == 50:
            values[9:12] = [words[39], words[41], words[42]]
        saved_events[str(uid)] = values
    return dict(bytes=len(payload), sha256=sha(payload), wire=wire, rigid=row[80:114],
        player=row[114:117], handles=[row[5], row[8]], events=saved_events)


def validate(guest, recipe, phase, payload=None, saved=None):
    healthy(guest, phase)
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES}
    for row in rows.values():
        check_identity(row, recipe)
    motion = check_motion(guest, recipe, phase)
    if phase == 1:
        require(all(row[22] == 1 and row[12] == 0 for row in rows.values()), 'Original pursuit ended early')
        require(guest['checkpoint_state'][4] == len(payload), 'Save byte count differs')
        return dict(result='PASS', motion=motion, saved=saved_state(payload, rows[119]))
    first = rows[0]
    require(guest['checkpoint_state'][1] == saved['bytes'] and first[16:80] == saved['wire'] and
        first[80:114] == saved['rigid'] and first[114:117] == saved['player'] and
        [first[5], first[8]] == saved['handles'] and first[117] == 2, 'Fresh exact owner/rigid/order/player restore differs')
    restore = guest['extra']['rf_scene_secondary_vehicle_pool_restore'][:80]
    require(restore[:8] == [1, OWNER, first[5], 0, 5, 0, GOTO, 1] and
        restore[8:72] == saved['wire'] and restore[72:75] == [1, 2, 1], 'Staged assignment differs from saved row')
    clock = guest['extra']['rf_scene_secondary_fighter_pursuit_clocks'][0]
    expected = {}
    for uid, values in saved['events'].items():
        values = values.copy()
        for at in (4, 9):
            if values[at] == NONE:
                continue
            delta = values[at]-clock
            if delta > apc.TIMER_PERIOD//2:
                delta -= apc.TIMER_PERIOD
            elif delta < -apc.TIMER_PERIOD//2:
                delta += apc.TIMER_PERIOD
            values[at] = delta & NONE
        expected[int(uid)] = values
    require({first[at]: first[at:at+12] for at in range(152, 212, 12)} == expected,
        'Fresh exact event/timer restore differs')
    require(rows[40][22] == 1 and rows[78][22] == rows[118][22] == 0 and rows[118][12] == 1 and
        rows[78][120] == rows[118][120], 'Ordinary OFF did not stop pursuit command sampling')
    return dict(result='PASS', exact_restore=True, ordinary_OFF=True, motion=motion)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare-only', type=Path)
    mode.add_argument('--run-root', type=Path)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--resume-source', type=Path,
        help='Read-only revalidation of an existing completed source report, then run only fresh load')
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    require(not args.resume_source or args.run_root, '--resume-source requires --run-root')
    require(30 <= args.seconds <= 3600, 'Require 30..3600 seconds')
    folder = args.prepare_only or args.run_root
    folder.mkdir(parents=True, exist_ok=args.prepare_only is not None)
    level, recipe = prepare_level(folder / 'level', args.source_root)
    if args.prepare_only:
        print(json.dumps(dict(result='PREPARED_NOT_RUN', path=str(level), recipe=recipe), indent=2))
        return
    require_no_project_xemu(ROOT)
    hdd = prepare_hdd(ROOT, ROOT / 'local/xemu-harness/pacing-base.qcow2')
    DISC.mkdir(parents=True, exist_ok=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', CONFIG}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe, phases={}, validation={}, binaries={},
        input_sha256={}, original_input_sha256={name: sha(data) if data is not None else None for name, data in original.items()},
        limitations=LIMITS, hdd=str(hdd))
    saved = None
    phases = ((1, 'source'), (2, 'loaded'))
    if args.resume_source:
        prior = json.loads(args.resume_source.read_text())
        require(prior['recipe']['archive_sha256'] == recipe['archive_sha256'] and
            Path(prior['hdd']).resolve() == hdd.resolve(), 'Source fixture/HDD identity changed')
        for name in ('main.exe', 'main.map'):
            require(sha((ROOT / 'build/xbox' / name).read_bytes()) == prior['binaries']['source'][name]['sha256'],
                'Source and continuation compiled binary differ: ' + name)
        payload = (args.resume_source.parent / 'source/xbox-world.rfwc').read_bytes()
        report['validation']['source'] = validate(prior['phases']['source'], recipe, 1, payload)
        saved = report['validation']['source']['saved']
        report['phases']['source'] = prior['phases']['source']
        report['binaries']['source'] = prior['binaries']['source']
        report['input_sha256']['source'] = prior['input_sha256']['source']
        report['resumed_source_report'] = str(args.resume_source.resolve())
        report['resumed_source_report_sha256'] = sha(args.resume_source.read_bytes())
        phases = ((2, 'loaded'),)
    restore = folder / 'input-restore'; restore.mkdir()
    for name, data in original.items():
        if data is not None:
            (restore / name).write_bytes(data)
    (restore / 'manifest.json').write_text(json.dumps(report['original_input_sha256'], indent=2) + '\n')

    def write_report():
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    try:
        for phase, label in phases:
            inputs = phase_inputs(level, phase)
            for name in names:
                (DISC / name).unlink(missing_ok=True)
            for name, data in inputs.items():
                require(name in names, 'Untracked disc input ' + name)
                (DISC / name).write_bytes(data)
            report['input_sha256'][label] = {name: sha(data) for name, data in inputs.items()}
            write_report(); build(folder, label)
            report['binaries'][label] = preserve_launch_binaries(folder / (label+'-binaries'))
            if phase == 2:
                for name in ('main.exe', 'main.map'):
                    require(report['binaries'][label][name]['sha256'] == report['binaries']['source'][name]['sha256'],
                        'Freshly built source/loaded binary differs: ' + name)
            guest = run_guest(folder, label, hdd, FRAMES, args.seconds, snapshot=phase == 2,
                capture_world=phase == 1, extra_symbols=SYMBOLS, allow_guest_error=True)
            report['phases'][label] = guest; write_report()
            payload = (folder / label / 'xbox-world.rfwc').read_bytes() if phase == 1 else None
            report['validation'][label] = validate(guest, recipe, phase, payload, saved)
            if phase == 1:
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
            report['result'] = 'FAIL'; report['restore_build_error'] = str(exc)
            raise
        finally:
            report['restored_input_sha256'] = {name: sha((DISC / name).read_bytes()) if (DISC / name).exists() else None for name in sorted(names)}
            report['disc_restored'] = report['original_input_sha256'] == report['restored_input_sha256']
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            write_report(); print(folder, report['result'], flush=True)
        require(report['disc_restored'], 'Original disc inputs were not restored exactly')


if __name__ == '__main__':
    main()
