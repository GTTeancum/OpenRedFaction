"""Bounded Xbox rotating PLAYER support ordinary save and fresh-load checks.

Reuses the accepted enemy-free L20S2 fixture: byte-exact authored Fighter4717,
explicit off-center roof seed40, original UnHide0 and When_Dead60, +Y slow yaw.
Source150 never jumps. Fresh ground20 continues its arc; separate jump40 uses
the first post-load input (frame1) and checks the exact saved additive cache.
An unchanged retained RFEN6 NPC save gets one optical compatibility load20.
No campaign routes, PC runtime, images, guest memory writes or host input.

--prepare-only writes disposable metadata only. Builds, disc staging, emulator
runs and final generated-artifact cleanup belong to the parent operator.
"""

import argparse
import datetime
import json
import math
from pathlib import Path
import struct

from xemu_ai_projectile_save import fixture as storage_fixture
from xemu_native_world_save import ROOT, DISC, FLAGS, address, build, run_guest
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd
import xemu_vehicle_rotating_support as rotation


SOURCE_FRAMES, GROUND_FRAMES, JUMP_FRAMES, LEGACY_FRAMES = 150, 20, 40, 20
JUMP_FRAME = 1  # Ordinary startup restore occurs after frame-zero physics.
OBSERVATION = 'rf_scene_player_support_save_fixture'
AUDIT = 'rf_scene_player_support_save_audit'
SYMBOLS = dict(rotation.SYMBOLS, **{OBSERVATION: 7 * 32, AUDIT: 32})
OBSERVATION_FRAMES = (0, 1, 2, 5, 18, 38, 148)
LEGACY_ROOT = ROOT / 'artifacts/xemu/vehicle-rotating-support-20261007-125608'
LEGACY_WORLD_SHA256 = 'aadd7ab693ff2dfc35366218e232cfb1641371a14d3278117f06dca35f92755e'
LEGACY_ARCHIVE_SHA256 = 'b8751e780819099d3d5c46247a3a3d9b803dcf43d55bd736085680ecd642ce2e'
LIMITATIONS = ('One explicitly seeded, idle, off-center player on a passive attached Fighter, '
    'gentle yaw only; no natural landing, walking, pitch/roll, crush, active vehicle or campaign claim. '
    'Legacy compatibility uses a genuine unchanged RFEN6 NPC fixture save, not a fabricated older player save.')


def replay(frames, jump_frame=None):
    if jump_frame is not None and not 0 <= jump_frame < frames - 1:
        raise ValueError('Jump must occur during a simulated input frame')
    return b'RFI6' + struct.pack('<I', 48) + b''.join(struct.pack('<5f7I',
        0, 0, 0, 0, 0, 0, int(frame == jump_frame), 0, 0, 0, 0, 0)
        for frame in range(frames))


def probe(monitor, mapping):
    result = {name: words(monitor, address(mapping, name), count)
              for name, count in SYMBOLS.items()}
    result['frame'] = words(monitor, address(mapping, 'rf_diagnostic'), 58)[37]
    return result


def observed(sample, frame):
    at = OBSERVATION_FRAMES.index(frame) * 32
    row = sample[OBSERVATION][at:at + 32]
    if len(row) != 32 or row[:2] != [1, frame] or not row[2] or \
            not row[15] or row[15] == rotation.NO_HANDLE or row[2] == row[15] or \
            row[16] != rotation.PUSHER or rotation.f(row[29]) != 2000. or row[30] & 0x4000:
        raise RuntimeError('Missing exact living player/owner observation at frame%d: %r' % (frame, row))
    return row


def decoded(row):
    return dict(frame=row[1], player_handle=row[2], support_handle=row[3], mode=row[4],
        body_flags=row[5], position=rotation.xyz(row, 6), body_velocity=rotation.xyz(row, 9),
        support_velocity=rotation.xyz(row, 12), support_velocity_bits=row[12:15],
        host_handle=row[15], host_uid=row[16], center=rotation.xyz(row, 17),
        basis=[rotation.f(v) for v in row[20:29]], jump_count=row[31])


def healthy_guest(guest, frames, loading=False, payload_bytes=None):
    if guest['guest_phase'] != 5 or guest['frames'] != frames or \
            guest['memory_bytes'] != 64 * 1024 * 1024 or guest['free_pages'] <= 0 or guest['player_life'][2]:
        raise RuntimeError('Incomplete living stock64MiB bounded phase')
    state = guest['checkpoint_state']
    if loading and (state[8] != 1 or state[0] or state[1] != payload_bytes or state[9]):
        raise RuntimeError('Ordinary fresh load failed or unexpectedly saved: %r' % state)
    for sample in (guest.get('probe'), guest.get('final_probe'), guest['extra']):
        if sample is None:
            continue
        damage, combat = sample['rf_scene_passive_damage'], sample['rf_scene_enemy_combat']
        if damage[2] or damage[3] or damage[6] or damage[7] != 1 or combat[2] or combat[3] or combat[7]:
            raise RuntimeError('Enemy-free fixture caused combat or vehicle damage')
        if loading and (any(sample['rf_scene_setup_result']) or any(sample['rf_scene_passive_roof_fixture'])):
            raise RuntimeError('Fresh load replayed setup or explicit support seed')
        if loading and any(sample[AUDIT]):
            raise RuntimeError('Fresh load unexpectedly reran the source-only codec audit')


def validate_audit(sample):
    actual = sample[AUDIT]
    expected_status = [value & 0xffffffff for value in
        (0, -3, -2, -2, -3, 0, 0, -2, -2, -2, 0, 0, -2, -2, -2, -2)]
    expected_summary = [1, SOURCE_FRAMES - 2, 16, 0xffff, 0, 0, 244, 7,
        rotation.PUSHER, 1, 7, 96, 31, 1, 1, 0]
    if actual != expected_summary + expected_status:
        raise RuntimeError('Native read-only admission/codec audit failed: %r' % actual)
    return dict(result='PASS', frame=actual[1], checks=actual[2], pass_mask=hex(actual[3]),
        environment_bytes=actual[6], malformed_outputs_stayed_null=True,
        temporary_stages_closed=True,
        checks_exercised=['exact accepted rotating point', 'wrong full handle generation',
            'nonfinite point', 'out-of-range point', 'unchanged legacy player-save scope',
            'RFEN7 local encode', 'RFEN7 exact cache roundtrip', 'RFEN7 NaN cache rejection',
            'RFEN7 out-of-range cache rejection', 'RFEN7 zero support UID rejection',
            'actual RFVA2 scratch capture/decode', 'eligible saved host', 'wrong saved UID',
            'detached saved host', 'hidden saved host', 'dead saved host'])


def grounded(row):
    if row[3] != row[15] or row[4] != 1 or not row[5] & 0x400000 or row[31]:
        raise RuntimeError('Expected grounded exact-owner player without a jump: %r' % row)


def validate_source(guest, recipe):
    healthy_guest(guest, SOURCE_FRAMES)
    end = guest['extra']
    seed = end['rf_scene_passive_roof_fixture']
    if seed[:3] != [rotation.PUSHER, rotation.PLACE_FRAME, 1] or seed[9] != 8:
        raise RuntimeError('Missing single explicit player roof seed')
    if any(end['rf_scene_passive_support_release']) or any(end['rf_scene_player_jump']):
        raise RuntimeError('Source must remain grounded through its ordinary save')
    initial = rotation.xyz(seed, 3)
    radius = math.hypot(initial[0] - recipe['center'][0], initial[2] - recipe['center'][2])
    if radius < .49:
        raise RuntimeError('Source support seed is not meaningfully off-axis')
    result = {}
    for label, sample in (('pre', guest['probe']), ('post', guest['final_probe']), ('end', end)):
        if sample['rf_scene_setup_result'] != [2, rotation.START_EVENT, 16, 0]:
            raise RuntimeError('Expected only original reveal0/start60 setup outputs')
        result[label] = rotation.validate_supported_sample(sample, recipe)
        value = result[label]
        expected = rotation.point_target(initial, recipe['center'], recipe['initial_basis'],
            value['accepted']['center'], value['accepted']['basis'])
        rotation.close_vector([value['position'][i] for i in (0, 2)],
            [expected[i] for i in (0, 2)], .025, 'grounded player cumulative yaw arc')
        actual_radius = math.hypot(value['position'][0] - recipe['center'][0],
            value['position'][2] - recipe['center'][2])
        if abs(actual_radius - radius) > .025:
            raise RuntimeError('Grounded player changed its roof radius')
    if not 110 <= result['pre']['frame'] < 125 or not 140 <= result['post']['frame'] < SOURCE_FRAMES - 1:
        raise RuntimeError('Source probes missed their bounded windows')
    row = observed(end, SOURCE_FRAMES - 2)
    grounded(row)
    live, _, _ = rotation.checked_live(end, recipe)
    if row[6:9] != live[6:9] or row[12:15] != live[9:12]:
        raise RuntimeError('Source observer differs from final physics/live support state')
    controller = end['rf_scene_rotating_doors']
    if controller[0] != SOURCE_FRAMES - rotation.START_FRAME - 1 or controller[1] or \
            controller[2] != rotation.LIFT_KEY or controller[7] or \
            abs(rotation.f(controller[3]) - controller[0] * recipe['radians_per_tick']) > .0001:
        raise RuntimeError('Source controller timing is not final physics frame148')
    result.update(result='PASS', initial_position=initial, radius=radius,
        source148=decoded(row), admission_codec_audit=validate_audit(end), free_pages=guest['free_pages'])
    return result


def validate_saved(guest, payload, recipe):
    state = guest['checkpoint_state']
    if state[9] != 1 or state[3] or state[4] != len(payload):
        raise RuntimeError('Ordinary save did not admit grounded rotating player: %r' % state)
    sections = rotation.checkpoint_sections(payload)
    player, mover, vehicle, environment = (sections[k] for k in (1, 3, 11, 16))
    if player[:4] != b'RFPL' or len(player) != 544 or struct.unpack_from('<I', player, 4)[0] not in (1, 2, 4):
        raise RuntimeError('Player base format unexpectedly changed')
    if environment[:4] != b'RFEN' or struct.unpack_from('<I', environment, 4)[0] != 7 or \
            struct.unpack_from('<I', environment, 76)[0] != rotation.PUSHER:
        raise RuntimeError('Expected identity-bound RFEN7 exact player support cache')
    if environment[24:56] != payload[24:56]:
        raise RuntimeError('RFEN7 is not bound to the ordinary world identity')
    # RFEN7 tail interpretation is intentionally centralized for the codec contract.
    velocity_bits = support_tail(environment)
    if vehicle[:4] != b'RFVA' or struct.unpack_from('<I', vehicle, 4)[0] != 2:
        raise RuntimeError('Expected unchanged RFVA2 passive-owner component')
    host_bytes, count = struct.unpack_from('<2I', vehicle, 8)
    if count != 1 or len(vehicle) != 16 + host_bytes + 80:
        raise RuntimeError('Expected exactly one saved passive owner')
    owner = vehicle[16 + host_bytes:]
    if struct.unpack_from('<2I', owner) != (rotation.PUSHER, 1) or \
            struct.unpack_from('<2f', owner, 56) != (2000., 0.):
        raise RuntimeError('Saved passive owner identity, attachment or health changed')
    if mover[:4] != b'RFMC' or len(mover) != 152 or struct.unpack_from('<I', mover, 4)[0] != 1 or \
            struct.unpack_from('<I', mover, 16)[0] != 1 or struct.unpack_from('<I', mover, 64)[0] != rotation.LIFT_KEY:
        raise RuntimeError('Expected unchanged one-controller RFMC1')
    phase, speed, angle = struct.unpack_from('<3f', mover, 104)
    if not 0 < phase < rotation.DURATION or not all(math.isfinite(v) for v in (phase, speed, angle)):
        raise RuntimeError('Saved controller is not mid-rotation')
    row = observed(guest['extra'], SOURCE_FRAMES - 2)
    grounded(row)
    if list(struct.unpack_from('<3I', player, 32)) != row[6:9] or \
            list(struct.unpack_from('<3I', player, 68)) != row[9:12] or velocity_bits != row[12:15] or \
            list(struct.unpack_from('<3I', owner, 8)) != row[17:20] or \
            list(struct.unpack_from('<9I', owner, 20)) != row[20:29]:
        raise RuntimeError('Saved player/cache/owner pose differs from exact source148 bits')
    if math.hypot(rotation.f(velocity_bits[0]), rotation.f(velocity_bits[2])) <= .05:
        raise RuntimeError('Saved support cache lacks measurable angular point velocity')
    result = decoded(row)
    result.update(position_bits=row[6:9], body_velocity_bits=row[9:12], center_bits=row[17:20],
        basis_bits=row[20:29], controller_angle=angle, controller_phase=phase,
        bytes=len(payload), sha256=rotation.sha(payload), formats=['RFPL', 'RFVA2', 'RFMC1', 'RFEN7'])
    return result


def support_tail(environment):
    """RFEN7 adds exact binary32 support velocity after the RFEN6 clock tail."""
    if len(environment) < 244 or struct.unpack_from('<I', environment, 8)[0] != len(environment):
        raise RuntimeError('Truncated RFEN7 support cache')
    force_count, nav_count = struct.unpack_from('<2I', environment, 16)
    monitor_at = 80 + force_count * 16 + nav_count * 8 + 96 + 24
    if monitor_at + 24 + 20 > len(environment):
        raise RuntimeError('RFEN7 monitor/replay/cache tail exceeds envelope')
    monitor_count = struct.unpack_from('<I', environment, monitor_at)[0]
    if monitor_at + 24 + monitor_count * 20 + 20 != len(environment) or \
            struct.unpack_from('<I', environment, len(environment) - 20)[0] >= 30:
        raise RuntimeError('RFEN7 tail changed the RFEN6 monitor or replay layout')
    checksum = 2166136261
    for at, byte in enumerate(environment):
        checksum = ((checksum ^ (0 if 12 <= at < 16 else byte)) * 16777619) & 0xffffffff
    if checksum != struct.unpack_from('<I', environment, 12)[0]:
        raise RuntimeError('RFEN7 component checksum mismatch')
    bits = list(struct.unpack_from('<3I', environment, len(environment) - 12))
    if not all(math.isfinite(rotation.f(v)) and abs(rotation.f(v)) <= 10000 for v in bits):
        raise RuntimeError('Saved support cache is nonfinite or out of range')
    return bits


def validate_restored(guest, saved):
    row = observed(guest['extra'], 0)
    grounded(row)
    if row[6:9] != saved['position_bits'] or row[9:12] != saved['body_velocity_bits'] or \
            row[12:15] != saved['support_velocity_bits'] or row[17:20] != saved['center_bits'] or \
            row[20:29] != saved['basis_bits']:
        raise RuntimeError('Frame0 restore lost exact player position, momentum/cache or owner pose')
    return decoded(row)


def validate_ground_load(guest, saved, recipe):
    healthy_guest(guest, GROUND_FRAMES, True, saved['bytes'])
    result = dict(restored0=validate_restored(guest, saved))
    for frame in (1, 2, 5, GROUND_FRAMES - 2):
        row = observed(guest['extra'], frame)
        grounded(row)
        if (row[2], row[15]) != (result['restored0']['player_handle'], result['restored0']['host_handle']):
            raise RuntimeError('Fresh-load owner handles changed during continuation')
        actual = decoded(row)
        expected = rotation.point_target(saved['position'], saved['center'], saved['basis'],
            actual['center'], actual['basis'])
        rotation.close_vector([actual['position'][i] for i in (0, 2)],
            [expected[i] for i in (0, 2)], .025, 'fresh-load continued player yaw arc')
        result['frame%d' % frame] = actual
    result['end_live'] = rotation.validate_supported_sample(guest['extra'], recipe)
    end = result['frame18']
    if rotation.norm([end['position'][i] - saved['position'][i] for i in (0, 2)]) < .025:
        raise RuntimeError('Fresh-loaded player did not continue its angular travel')
    controller = guest['extra']['rf_scene_rotating_doors']
    if controller[0] != GROUND_FRAMES - 2 or controller[1] or controller[7] or \
            abs(rotation.f(controller[3]) - saved['controller_angle'] - controller[0] * recipe['radians_per_tick']) > .0001:
        raise RuntimeError('Fresh-load controller reset or replayed its saved interval')
    if any(guest['extra']['rf_scene_passive_support_release']) or any(guest['extra']['rf_scene_player_jump']):
        raise RuntimeError('Ground continuation released or jumped')
    result.update(result='PASS', free_pages=guest['free_pages'])
    return result


def validate_jump_load(guest, saved, recipe):
    healthy_guest(guest, JUMP_FRAMES, True, saved['bytes'])
    result = dict(restored0=validate_restored(guest, saved))
    first = observed(guest['extra'], JUMP_FRAME)
    release = guest['extra']['rf_scene_passive_support_release']
    expected_release = [1, JUMP_FRAME, result['restored0']['host_handle'], result['restored0']['player_handle']]
    if release[:4] != expected_release or release[4:7] != saved['support_velocity_bits'] or release[7]:
        raise RuntimeError('Immediate first-postload jump did not release the exact saved cache')
    jump = guest['extra']['rf_scene_player_jump']
    if jump[0] != 1 or jump[1] != 1 or jump[3] != JUMP_FRAME:
        raise RuntimeError('Expected exactly one accepted first-postload jump')
    if first[3] or first[4] != 3 or first[5] & 0x400000 or first[31] != 1 or \
            first[12:15] != saved['support_velocity_bits']:
        raise RuntimeError('First jump frame lost its airborne mode or saved additive velocity')
    first_value = decoded(first)
    expected_delta = [(saved['body_velocity'][i] + saved['support_velocity'][i]) / 60 for i in (0, 2)]
    actual_delta = [first_value['position'][i] - saved['position'][i] for i in (0, 2)]
    rotation.close_vector(actual_delta, expected_delta, .0003, 'first jump integrated saved horizontal momentum')
    if first_value['body_velocity'][1] <= 0 or first_value['position'][1] <= saved['position'][1]:
        raise RuntimeError('Accepted immediate jump lacks upward body motion')
    result['frame1'] = first_value
    for frame in (2, 5):
        row = observed(guest['extra'], frame)
        if row[3] or row[4] != 3 or row[5] & 0x400000 or row[31] != 1 or row[12:15] != saved['support_velocity_bits']:
            raise RuntimeError('Early airborne continuation altered saved additive velocity')
        value = decoded(row)
        elapsed = frame / 60
        expected = [saved['position'][i] +
            (saved['body_velocity'][i] + saved['support_velocity'][i]) * elapsed for i in (0, 2)]
        rotation.close_vector([value['position'][i] for i in (0, 2)], expected, .001,
            'early airborne tangent follows saved momentum without renewed yaw carry')
        result['frame%d' % frame] = value
    # Later landing is ordinary gameplay. The assertion concerns the exact
    # first post-load jump and its early airborne momentum, not forced flight.
    result['end38'] = decoded(observed(guest['extra'], JUMP_FRAMES - 2))
    result.update(result='PASS', release_velocity=rotation.xyz(release, 4),
        expected_first_delta=expected_delta, actual_first_delta=actual_delta, free_pages=guest['free_pages'])
    return result


def prepare_legacy(folder, legacy_root):
    path = legacy_root / 'npc/source/xbox-world.rfwc'
    payload = path.read_bytes()
    if rotation.sha(payload) != LEGACY_WORLD_SHA256:
        raise RuntimeError('Retained accepted legacy save hash changed')
    report = json.loads((legacy_root / 'report.json').read_text())
    old = report['cases']['npc']
    if report['result'] != 'PASS' or old['recipe']['archive_sha256'] != LEGACY_ARCHIVE_SHA256 or \
            old['source']['world_checkpoint_sha256'] != LEGACY_WORLD_SHA256:
        raise RuntimeError('Retained legacy provenance no longer matches the accepted report')
    sections = rotation.checkpoint_sections(payload)
    if sections[16][:8] != b'RFEN' + struct.pack('<I', 6):
        raise RuntimeError('Retained legacy payload is not genuine RFEN6')
    level, recipe = rotation.prepare_level(folder, 'npc')
    if recipe['archive_sha256'] != LEGACY_ARCHIVE_SHA256 or recipe['fixture_sha256'] != old['recipe']['fixture_sha256']:
        raise RuntimeError('Regenerated legacy level differs from accepted source identity')
    # Recheck the old save against its own source telemetry without modifying it.
    saved = rotation.validate_saved(old['source'], payload, recipe)
    return level, recipe, payload, saved, dict(source=str(path), bytes=len(payload),
        sha256=LEGACY_WORLD_SHA256, archive_sha256=LEGACY_ARCHIVE_SHA256, environment='RFEN6',
        transport='Unchanged payload wrapped in RFSG1 at optical world-fixture.0; ordinary read-only loader')


def base_inputs(level, recipe, frames):
    result = rotation.source_inputs(level, recipe)
    for name in ('campaign-setup.bin', 'campaign-passive-roof.bin'):
        del result[name]
    result['player-replay.bin'] = replay(frames)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--legacy-root', type=Path, default=LEGACY_ROOT)
    parser.add_argument('--seconds', type=int, default=480)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('--seconds must be positive')
    folder = args.prepare_only or ROOT / 'artifacts/xemu' / (
        'player-rotating-support-save-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True, exist_ok=args.prepare_only is not None)
    level, recipe = rotation.prepare_level(folder / 'player-level', 'player')
    legacy_level, legacy_recipe, legacy_payload, legacy_saved, legacy_evidence = prepare_legacy(
        folder / 'legacy-level', args.legacy_root)
    if args.prepare_only:
        print(json.dumps(dict(result='PASS', status='PREPARED_NOT_RUN', folder=str(folder),
            player_archive_sha256=recipe['archive_sha256'], legacy=legacy_evidence,
            frames=dict(source=SOURCE_FRAMES, ground=GROUND_FRAMES, jump=JUMP_FRAMES, legacy=LEGACY_FRAMES),
            limitations=LIMITATIONS), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = prepare_hdd(ROOT, base)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
    report = dict(result='FAIL', scope=__doc__, limitations=LIMITATIONS, recipe=recipe,
        legacy=legacy_evidence, phases={}, binaries={}, input_sha256={},
        original_input_sha256={name: rotation.sha(data) if data is not None else None for name, data in original.items()})
    # Retain exact pre-task files so an interrupted parent can restore its own
    # inputs. Per-phase binary evidence still captures the actual launched build.
    restore = folder / 'input-restore'
    restore.mkdir()
    for name, data in original.items():
        if data is not None:
            (restore / name).write_bytes(data)
    (restore / 'manifest.json').write_text(json.dumps(report['original_input_sha256'], indent=2) + '\n')

    def stage(label, inputs):
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        for name, data in inputs.items():
            if name not in names:
                raise RuntimeError('Untracked fixture input ' + name)
            (DISC / name).write_bytes(data)
        report['input_sha256'][label] = {name: rotation.sha(data) for name, data in inputs.items()}
        build(folder, label)
        report['binaries'][label] = rotation.preserve_launch_binaries(folder / (label + '-binaries'))

    try:
        inputs = rotation.source_inputs(level, recipe)
        inputs['player-replay.bin'] = replay(SOURCE_FRAMES)
        inputs['world-hdd-save.flag'] = b'1'
        stage('source', inputs)
        source = run_guest(folder, 'source', hdd, SOURCE_FRAMES, args.seconds,
            capture_world=True, extra_symbols=SYMBOLS, probe=probe, probe_frame=110,
            final_probe=probe, final_probe_frame=140, allow_guest_error=True)
        report['phases']['source'] = source
        report['source_validation'] = validate_source(source, recipe)
        payload = (folder / 'source/xbox-world.rfwc').read_bytes()
        saved = report['saved'] = validate_saved(source, payload, recipe)
        for label, frames, jumping in (('ground', GROUND_FRAMES, False), ('jump', JUMP_FRAMES, True)):
            inputs = base_inputs(level, recipe, frames)
            inputs['world-hdd-load.flag'] = b'1'
            inputs['player-replay.bin'] = replay(frames, JUMP_FRAME if jumping else None)
            stage(label, inputs)
            loaded = run_guest(folder, label, hdd, frames, args.seconds, snapshot=True,
                extra_symbols=SYMBOLS, allow_guest_error=True)
            report['phases'][label] = loaded
            report[label + '_validation'] = (validate_jump_load if jumping else validate_ground_load)(loaded, saved, recipe)
        inputs = base_inputs(legacy_level, legacy_recipe, LEGACY_FRAMES)
        inputs['world-fixture-load.flag'] = b'1'
        inputs['world-fixture.0'] = storage_fixture(legacy_payload)
        stage('legacy', inputs)
        legacy = run_guest(folder, 'legacy', base, LEGACY_FRAMES, args.seconds, snapshot=True,
            extra_symbols=SYMBOLS, probe=probe, probe_frame=5, final_probe=probe,
            final_probe_frame=15, allow_guest_error=True)
        report['phases']['legacy'] = legacy
        healthy_guest(legacy, LEGACY_FRAMES, True, len(legacy_payload))
        report['legacy_validation'] = rotation.validate_loaded(legacy, legacy_saved, legacy_recipe)
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
            report['restored_input_sha256'] = {name: rotation.sha((DISC / name).read_bytes())
                if (DISC / name).exists() else None for name in sorted(names)}
            report['disc_restored'] = report['original_input_sha256'] == report['restored_input_sha256']
            if not report['disc_restored']:
                report['result'] = 'FAIL'
            (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(folder, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Original staged disc input hashes were not restored')


if __name__ == '__main__':
    main()
