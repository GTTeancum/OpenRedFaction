"""Parent-only hourly Xbox contact/release check at original L3S1 grate UID6913.

Run after the parent's explicit Xbox build. No compile, PC run, modified level,
placement override, injected event/inventory, host input or image capture.
The bounded ordinary replay pushes toward the original grate from the original
spawn. A sampled owner-qualified mesh contact is narrower than proving a fully
blocked passage or prop-top save support. --destroy adds one ordinary aimed
pistol shot and a second push through the former edge, within the same run.
"""
import argparse
import datetime
import io
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess

# Reuse the already-existing platform defaults, native runner and borrowed lock.
from xemu_fighter_grounded_exit import BorrowedSessionLock, floats, sha256, write_json
import xemu_native_world_save as native
from xemu_guest_snapshot import words
from xemu_host import SessionLock
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import standalone
from build_fragment_platform_fixture import read_entry
from inspect_levels import inspect as inspect_level
from inspect_clutter_records import inspect as inspect_clutter

ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
ISO = ROOT / 'build/xbox/redfaction-diagnostic.iso'
MAP = ROOT / 'build/xbox/main.map'
XBE = DISC / 'default.xbe'
HDD = ROOT / 'local/xemu-harness/pacing-base.qcow2'
UID = 6913
FRAMES, MOVE_BEGIN, MOVE_END = 240, 60, 180
AIM_BEGIN, AIM_END, FIRE_FRAME = 240, 300, 330
RELEASE_BEGIN, RELEASE_END, DESTROY_FRAMES = 360, 480, 540
TARGET_YAW, TARGET_PITCH, LOOK_RATE = -.05, -.35, 2.0
UINT_MAX = 0xffffffff
SYMBOLS = {
    'rf_scene_actor_body_sweeps': 5, 'rf_scene_actor_body_contact': 23,
    'rf_scene_actor_tick_stats': 8, 'rf_scene_actor_landing': 8,
    'rf_scene_player_life': 8, 'rf_scene_player_spawn_diagnostic': 19,
    'rf_scene_player_ammo': 8, 'rf_scene_combat': 8,
    'rf_scene_clutter_damage': 8, 'rf_scene_clutter_collision': 9,
    'rf_scene_clutter': 8,
}
LIVE_SYMBOLS = ('campaign_clutter_bodies', 'campaign_registry', 'scene_actor_body',
                'campaign_pistol_id', 'campaign_player_inventory',
                'rf_scene_actor_ring_frames', 'rf_scene_actor_render_frames')


def authored_recipe():
    """Read original records only; no archive extraction or generated scene."""
    payload = read_entry(ROOT / 'Installed_Game/levels1.vpp', 'L3S1.rfl')
    metadata = inspect_level(io.BytesIO(payload), dict(offset=0, size=len(payload), name='L3S1.rfl'))
    if metadata['version'] != 180:
        raise RuntimeError('Require original version180 L3S1')
    def section(kind):
        row = next(row for row in metadata['sections'] if row['type'] == hex(kind))
        return payload[row['offset'] + 8:row['offset'] + 8 + row['size']]
    records = inspect_clutter(section(0x50000))
    slots = [i for i, row in enumerate(records) if row['uid'] == UID]
    if len(slots) != 1:
        raise RuntimeError('Original grate UID is absent or ambiguous')
    slot = slots[0]
    target = records[slot]
    position = list(struct.unpack('<3f', target['position']))
    expected = (-53.0003433, -3.125, -19.2345066)
    if target['class_name'] != b'Duct Grate Cover' or any(abs(a-b) > .0001 for a,b in zip(position, expected)):
        raise RuntimeError('Unexpected original grate record')
    start = struct.unpack('<12f', section(0x70000))
    spawn, forward, right = start[:3], start[3:6], start[6:9]
    # level.c converts disk forward/right/up to runtime right/up/forward.
    # No yaw command: project the source-grounded horizontal target direction
    # into that unchanged body basis. This approaches the thin grate's edge.
    dx, dz = position[0] - spawn[0], position[2] - spawn[2]
    length = math.hypot(dx, dz)
    move = [.25 * (dx*right[0] + dz*right[2]) / length, 0,
            .25 * (dx*forward[0] + dz*forward[2]) / length]
    if not 2 < length < 3 or any(not math.isfinite(x) or abs(x) > 1 for x in move):
        raise RuntimeError('Original short-approach geometry changed')
    return dict(level='L3S1.rfl', archive='levels1.vpp', uid=UID, slot=slot,
        clutter_count=len(records), target_xyz=position, original_spawn=list(spawn),
        default_forward=list(forward), default_right=list(right), move=move,
        frames=FRAMES, neutral=[0, MOVE_BEGIN-1], push=[MOVE_BEGIN, MOVE_END-1],
        released_input=[MOVE_END, FRAMES-1], other_inputs='all zero',
        scope='Contact-only edge approach; reachability and successful contact are unverified.')


def destruction_recipe(recipe):
    # Installed player forms use class+64 Rot Acceleration=2.0; scene.c passes
    # it to rf_look_update_pose at 1/60. See PLAYER-AUTHORED-LOOK-RATE.md.
    # The successful 06:00 run stopped at (-52.97645,-3.11848,-20.58442).
    # A near +Z ray, angled slightly -X/down, meets the grate's near edge
    # inside its thin X slab. Center aiming would have less thickness margin.
    start_yaw = math.atan2(recipe['default_forward'][0], recipe['default_forward'][2])
    duration = (AIM_END - AIM_BEGIN) / 60
    look = [TARGET_PITCH / (LOOK_RATE * duration),
            (TARGET_YAW - start_yaw) / (LOOK_RATE * duration)]
    if any(not math.isfinite(v) or abs(v) > 1 for v in look):
        raise RuntimeError('Ordinary bounded turn cannot reach the intended aim')
    world = [sum(recipe['move'][j] * basis[k] for j, basis in
                 ((0, recipe['default_right']), (2, recipe['default_forward']))) for k in range(3)]
    right = [math.cos(TARGET_YAW), 0, -math.sin(TARGET_YAW)]
    forward = [math.sin(TARGET_YAW), 0, math.cos(TARGET_YAW)]
    move = [sum(world[k]*basis[k] for k in range(3)) if j != 1 else 0
            for j, basis in enumerate((right, (0, 1, 0), forward))]
    recipe.update(destroy=True, frames=DESTROY_FRAMES,
        aim=dict(frames=[AIM_BEGIN, AIM_END-1], look=look, yaw=TARGET_YAW,
                 pitch=TARGET_PITCH, rate=LOOK_RATE,
                 basis_source='src/core/eye.c rf_look_update / look_basis_seed; scene.c actor_listener_pose'),
        fire_frame=FIRE_FRAME, released_push=[RELEASE_BEGIN, RELEASE_END-1],
        released_move=move, world_move=world, final_neutral=[RELEASE_END, DESTROY_FRAMES-1],
        mesh_bounds=[[-53.0159071, -52.9847794], [-3.8720608, -2.3779392], [-19.9815674, -18.4874458]],
        other_inputs='one primary fire edge; no crouch, jump, Use, reload, cycle or alternate',
        scope='Original contact, one ordinary shot, same-owner retirement and ordinary movement past the former near edge; no full passage or campaign route claim.')
    return recipe


def replay(recipe):
    rows = []
    for frame in range(recipe['frames']):
        move = recipe['move'] if MOVE_BEGIN <= frame < MOVE_END else (0, 0, 0)
        look, fire = (0, 0), 0
        if recipe.get('destroy'):
            if AIM_BEGIN <= frame < AIM_END:
                look = recipe['aim']['look']
            if RELEASE_BEGIN <= frame < RELEASE_END:
                move = recipe['released_move']
            fire = int(frame == FIRE_FRAME)
        rows.append(struct.pack('<5f7I', *move, *look, 0, 0, 0, fire, 0, 0, 0))
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def live_reader(folder, recipe, samples, symbols):
    def read(monitor, mapping):
        get = lambda name, count: words(monitor, native.address(mapping, name), count)
        diag = get('rf_diagnostic', 58)
        if diag[2] != 2 or not diag[37]:
            return  # The guest can finish between the runner read and QMP stop.
        row = dict(frame=diag[37], raw={name: get(name, count) for name, count in symbols.items()})
        raw = row['raw']
        if raw['rf_scene_clutter'][1] != recipe['clutter_count']:
            raise RuntimeError('Live clutter census does not match original L3S1')
        owners = get('campaign_clutter_bodies', 1)[0]
        if not owners:
            raise RuntimeError('Original clutter owner array is absent')
        pointer = words(monitor, owners + 4*recipe['slot'], 1)[0]
        if not pointer:
            raise RuntimeError('Original grate has no retained native owner')
        # Xbox32 ABI from clutter.h: state108 + link8 + attachment16 + body324.
        # Fail closed on token/UID/material/registry mismatch, never infer owner
        # identity from the nearby render bound or contact point alone.
        owner = words(monitor, pointer, 133)
        handle = owner[2]
        if owner[0] != pointer or owner[114] != UID or not handle or (handle & 0xffff) >= 1024:
            raise RuntimeError('Native clutter ABI/UID/token does not match the expected owner')
        registry = words(monitor, native.address(mapping, 'campaign_registry') + 8*(handle & 0xffff), 2)
        if registry != [pointer, handle] or not owner[12]:
            raise RuntimeError('Grate has no exact generation-bearing registry/class identity')
        definition = words(monitor, owner[12], 11)
        body = get('scene_actor_body', 81)
        position = floats(body[22:25])
        if any(not math.isfinite(x) for x in position):
            raise RuntimeError('Player pose is nonfinite')
        if math.hypot(position[0]-recipe['original_spawn'][0], position[2]-recipe['original_spawn'][2]) > 5:
            raise RuntimeError('Player left the bounded five-unit original-spawn neighborhood')
        pistol = get('campaign_pistol_id', 1)[0]
        owned = struct.pack('<16I', *get('campaign_player_inventory', 16))
        contact = raw['rf_scene_actor_body_contact']
        ring_frames, render = get('rf_scene_actor_ring_frames', 64), get('rf_scene_actor_render_frames', 320)
        row.update(owner=dict(pointer=pointer, handle=handle, uid=owner[114], flags=owner[4],
                   health=floats(owner[9:10])[0], position=floats(owner[6:9]),
                   class_flags=definition[10], material=owner[115], class_material=definition[9],
                   extent=floats(owner[94:95])[0], physical=bool(owner[101] & 0x20)),
            player=dict(position=position, velocity=floats(body[46:49]), sphere_count=body[78],
                        pistol_id=pistol, pistol_owned=bool(pistol < 64 and owned[pistol])),
            contact=dict(owner=contact[12], material=contact[7], solid=contact[17],
                         sphere=contact[18], face=contact[20], fraction=floats(contact[6:7])[0],
                         point=floats(contact[:3]), normal=floats(contact[3:6]),
                         velocity=floats(contact[9:12])),
            position_ring=[dict(frame=ring_frames[i], position=floats(render[i*5+2:i*5+5]))
                           for i in range(64) if MOVE_BEGIN <= ring_frames[i] < diag[37]])
        if recipe.get('destroy'):
            # Existing eye ring is recorded before optional camera effects. It
            # grounds the planned ray; actual owner damage proves the shot hit.
            eye_base = native.address(mapping, 'rf_scene_actor_eye_frames')
            def eye_pose(frame):
                value = words(monitor, eye_base + (frame % 64)*46*4, 46)
                if value[0] != frame:
                    raise RuntimeError('Completed-frame eye ring was overwritten or has a different ABI')
                return dict(frame=frame, position=floats(value[25:28]), forward=floats(value[43:46]))
            row['eye'] = eye_pose(diag[37]-1)
            if FIRE_FRAME < diag[37] <= FIRE_FRAME + 64:
                row['shot_eye'] = eye_pose(FIRE_FRAME)
            look = get('actor_look', 32)
            row['look'] = dict(yaw=floats(look[6:7])[0], pitch=floats(look[8:9])[0])
        samples.append(row)
        # Incremental small evidence survives a later startup/runtime error.
        write_json(folder / 'live-samples.json', samples)
    return read


def evaluate(guest, samples, recipe):
    checks = dict(stock_64_mib=guest.get('memory_bytes') == 64*1024*1024,
        completed_replay=guest.get('guest_phase') == 5 and guest.get('frames') == recipe['frames'] and
                         guest.get('replay_state', [])[1:] == [recipe['frames'], recipe['frames'], 0],
        no_transition=guest.get('level_transitions', [None])[0] == 0,
        living=guest.get('player_life', [])[:3] == [0, 0, 0],
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        baseline_sample=any(row['frame'] < MOVE_BEGIN for row in samples),
        push_sample=any(MOVE_BEGIN <= row['frame'] < MOVE_END for row in samples))
    checks['original_spawn_retained'] = bool(samples) and all(
        floats(row['raw']['rf_scene_player_spawn_diagnostic'][1:4]) == recipe['original_spawn'] for row in samples)
    checks['real_unchanged_owned_pistol'] = bool(samples) and all(
        row['player']['pistol_owned'] and row['raw']['rf_scene_player_ammo'][:3] ==
        samples[0]['raw']['rf_scene_player_ammo'][:3] and
        row['raw']['rf_scene_player_ammo'][0] == row['player']['pistol_id'] and
        row['raw']['rf_scene_player_ammo'][2] == 16 and row['raw']['rf_scene_combat'][0] == 0 for row in samples)
    checks['authored_live_solid_metal_owner'] = bool(samples) and all(
        row['owner']['class_flags'] & 4 and not row['owner']['class_flags'] & 2 and
        not row['owner']['flags'] & (2 | 0x4000 | 0x40000) and row['owner']['health'] == 30 and
        row['owner']['material'] == row['owner']['class_material'] == 2 and
        row['owner']['extent'] > .5 and row['owner']['physical'] and
        row['owner']['position'] == recipe['target_xyz'] for row in samples)
    checks['no_collision_errors'] = bool(samples) and all(
        row['raw']['rf_scene_actor_body_sweeps'][3] == 0 and
        row['raw']['rf_scene_clutter_collision'][8] == 0 for row in samples)
    baseline = next((row for row in reversed(samples) if row['frame'] < MOVE_BEGIN), None)
    contacts = []
    for row in samples:
        contact = row['contact']
        if (not baseline or row['frame'] < MOVE_BEGIN or contact['owner'] != row['owner']['handle'] or
            baseline['contact']['owner'] == row['owner']['handle']):
            continue
        point = contact['point']
        if (contact['material'] == 2 and contact['solid'] == contact['face'] == UINT_MAX and
            contact['sphere'] < row['player']['sphere_count'] and 0 <= contact['fraction'] < 1 and
            contact['velocity'] == [0, 0, 0] and all(math.isfinite(v) for v in point+contact['normal']) and
            abs(point[0] - recipe['target_xyz'][0]) < .04 and
            -3.90 <= point[1] <= -2.35 and -20.01 <= point[2] <= -18.46 and
            row['raw']['rf_scene_actor_body_sweeps'][1] > baseline['raw']['rf_scene_actor_body_sweeps'][1] and
            row['raw']['rf_scene_actor_tick_stats'][3] > baseline['raw']['rf_scene_actor_tick_stats'][3]):
            contacts.append(dict(observed_frame=row['frame'], contact=contact, player=row['player']))
    failed = [name for name, passed in checks.items() if not passed]
    return dict(status='CHECK_FAILED' if failed else 'PASS_OWNER_QUALIFIED_CONTACT' if contacts else 'INCONCLUSIVE_NO_SAMPLED_GRATE_CONTACT',
        checks=checks, failed_checks=failed, owner_qualified_contacts=contacts,
        limits='Owner contact only. Latest body-hit telemetry can outlive a miss; observed_frame is not '
        'an exact impact timestamp. Unobserved brief contacts remain inconclusive. No claimed passage '
        'block/release, firing, destruction, weapon-only exclusion, prop-top support/save, visuals or FPS.')


def evaluate_destruction(guest, samples, recipe):
    result = evaluate(guest, [row for row in samples if row['frame'] < AIM_BEGIN], recipe)
    checks = result['checks']
    checks['original_contact_observed'] = bool(result['owner_qualified_contacts'])
    # Require sustained stasis during held input, rather than relying on the
    # last-contact record, which can persist after the contact has ended.
    before = {p['frame']: p['position'] for row in samples for p in row['position_ring']
              if 150 <= p['frame'] < MOVE_END}
    checks['held_push_blocked'] = len(before) >= 24 and all(
        max(p[k] for p in before.values())-min(p[k] for p in before.values()) < .02 for k in range(3))
    prefired = [row for row in samples if AIM_END <= row['frame'] <= FIRE_FRAME]
    checks['sampled_ordinary_aim'] = bool(prefired) and all(
        abs(row['look']['yaw']-TARGET_YAW) < .001 and abs(row['look']['pitch']-TARGET_PITCH) < .001
        and row['owner']['health'] == 30 and not row['owner']['flags'] & 2
        and row['raw']['rf_scene_combat'][0] == 0 for row in prefired)
    shot_eye = next((row['shot_eye'] for row in samples if 'shot_eye' in row), None)
    point = None
    if shot_eye and all(math.isfinite(v) for v in shot_eye['position']+shot_eye['forward']):
        eye, direction = shot_eye['position'], shot_eye['forward']
        if direction[2] > .5:
            distance = (recipe['mesh_bounds'][2][0]-eye[2])/direction[2]
            if 0 < distance < 2:
                point = [eye[k]+distance*direction[k] for k in range(3)]
    checks['shot_eye_intersects_old_near_edge'] = point is not None and all(
        recipe['mesh_bounds'][k][0]+.003 < point[k] < recipe['mesh_bounds'][k][1]-.003 for k in (0, 1))
    extra = guest.get('extra', {})
    damage, combat, ammo = (extra.get(key, []) for key in
                           ('rf_scene_clutter_damage', 'rf_scene_combat', 'rf_scene_player_ammo'))
    checks['one_owned_pistol_round'] = bool(samples) and len(combat) == len(ammo) == 8 and (
        combat[0] == 1 and combat[5] == 15 and combat[7] == 0 and ammo[2] == 15 and
        ammo[:2] == samples[0]['raw']['rf_scene_player_ammo'][:2] and ammo[3:6] == [0, 0, 0] and
        ammo[7] == 0 and all(row['player']['pistol_owned'] and
        row['raw']['rf_scene_player_ammo'][0] == row['player']['pistol_id'] for row in samples))
    checks['one_grate_damage_retirement'] = len(damage) == 8 and damage[1:5] == [1, 1, 1, UID] and floats(damage[5:6])[0] <= 0
    retired = [row for row in samples if FIRE_FRAME < row['frame'] < RELEASE_BEGIN and
               row['owner']['health'] <= 0 and row['owner']['flags'] & 2]
    checks['same_owner_retired_before_push'] = bool(retired) and bool(samples) and all(
        (row['owner']['pointer'], row['owner']['handle'], row['owner']['uid'], row['owner']['position']) ==
        (samples[0]['owner']['pointer'], samples[0]['owner']['handle'], UID, recipe['target_xyz']) for row in samples)
    checks['all_collision_reads_clean'] = bool(samples) and all(
        row['raw']['rf_scene_actor_body_sweeps'][3] == 0 and row['raw']['rf_scene_clutter_collision'][8] == 0 for row in samples)
    released = [row for row in samples if RELEASE_BEGIN < row['frame'] <= RELEASE_END and
                row['owner']['health'] <= 0 and row['owner']['flags'] & 2 and
                row['player']['position'][2] > recipe['mesh_bounds'][2][0]+.25 and
                abs(row['player']['position'][0]-recipe['target_xyz'][0]) < .25 and
                abs(row['player']['position'][1]+3.1184785) < .15]
    checks['ordinary_push_past_old_edge'] = bool(released)
    audio = extra.get('rf_scene_clutter_break_audio')
    if audio is not None:
        sound_hash = 2166136261
        for byte in b'metal_hit_metal3.wav':
            sound_hash = ((sound_hash ^ byte) * 16777619) & UINT_MAX
        checks['one_native_grate_break_start'] = (len(audio) == 10 and audio[:4] == [1, 1, 0, UID]
            and FIRE_FRAME <= audio[4] < RELEASE_BEGIN and audio[5] == 62 and
            audio[7] != UINT_MAX and audio[8] == sound_hash and audio[9] == 1)
    failed = [name for name, passed in checks.items() if not passed]
    result.update(status='CHECK_FAILED' if failed else 'PASS_OWNER_DESTRUCTION_RELEASE', failed_checks=failed,
        shot_eye=shot_eye, predicted_near_edge_intersection=point,
        retired_samples=retired, released_samples=released, break_audio=audio,
        limits='One original spawn-local contact/shot/retirement/near-edge release. Eye ring precedes optional '
        'camera effects; actual damage is required independently. Break-audio telemetry reports native request/start '
        'only, when available; no PCM or human-listening claim. No full passage, save, debris/VFX, FPS or campaign route claim.')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--seconds', type=int, default=600)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--destroy', action='store_true', help='Extend this same original-level replay with one shot and released push')
    args = parser.parse_args()
    if not 120 <= args.seconds <= 900:
        parser.error('--seconds must be in 120..900')
    folder = (args.out or ROOT / 'artifacts/xemu' / ('player-solid-prop-' +
        datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'))).resolve()
    if folder.exists():
        parser.error('Evidence directory already exists')
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (XBE, MAP, HDD, packer):
        if not path.is_file():
            raise RuntimeError('Missing parent-built input: ' + str(path))
    standalone(HDD)
    mapping = MAP.read_text()
    symbols = dict(SYMBOLS)
    if args.destroy:
        symbols['rf_scene_actor_initial_eye_offsets'] = 6
        # Parent-owned read-only audio diagnostic is optional; its absence
        # must not turn a physical release result into an audio-success claim.
        try:
            native.address(mapping, 'rf_scene_clutter_break_audio')
        except RuntimeError:
            pass
        else:
            symbols['rf_scene_clutter_break_audio'] = 10
        for name in ('actor_look', 'rf_scene_actor_eye_frames'):
            native.address(mapping, name)
    for name in tuple(symbols) + LIVE_SYMBOLS:
        native.address(mapping, name)
    recipe = authored_recipe()
    if args.destroy:
        recipe = destruction_recipe(recipe)
    require_no_project_xemu(ROOT)
    lock = SessionLock(ROOT)
    lock.acquire()
    old_lock_factory = native.SessionLock
    original = None
    iso_moved = staged = False
    report = dict(status='NOT_RUN', recipe=recipe, samples=[], source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip())
    try:
        require_no_project_xemu(ROOT)
        folder.mkdir(parents=True)
        # Check every input that could substitute original level/model/class data.
        for archive in ('levels1.vpp', 'tables.vpp', 'meshes.vpp'):
            if sha256(DISC / archive) != sha256(ROOT / 'Installed_Game' / archive):
                raise RuntimeError('Disc archive differs from original input: ' + archive)
        names = set(native.FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
        names |= {p.name for p in DISC.glob('*.flag') if p.is_file()}
        names |= {'player-control.flag', 'scene-preview.flag', 'audio-output.flag', 'scene-fixture.vpp',
                  'model-skin-test.bin', 'model-collision-test.bin', 'particle-step-fixtures.bin'}
        original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
        write_json(folder / 'disc-restore.json', {name: data.hex() if data is not None else None for name,data in original.items()})
        report['exact_inputs'] = dict(xbe=sha256(XBE), map=sha256(MAP), iso=sha256(ISO) if ISO.exists() else None)
        shutil.copyfile(XBE, folder / 'tested-default.xbe')
        shutil.copyfile(MAP, folder / 'tested-main.map')
        write_json(folder / 'recipe.json', recipe)
        if ISO.exists():
            ISO.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in original:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64,b'\0') + b'L3S1.rfl'.ljust(64,b'\0'))
        (DISC / 'scene-preview.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        if args.destroy:
            # Replay disables player_pacing, so the existing explicit flag is
            # required for native voice starts even with the host sink muted.
            (DISC / 'audio-output.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay(recipe))
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(DISC), str(ISO)], cwd=ROOT,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        if sha256(XBE) != report['exact_inputs']['xbe'] or sha256(MAP) != report['exact_inputs']['map']:
            raise RuntimeError('XBE/map changed after preservation')
        native.SessionLock = lambda root: BorrowedSessionLock(lock)
        guest = native.run_guest(folder, 'native', HDD, recipe['frames'], args.seconds, snapshot=True,
            extra_symbols=symbols, allow_guest_error=True, allow_player_dead=True,
            live_sample=live_reader(folder, recipe, report['samples'], symbols))
        report['native'] = guest
        report.update((evaluate_destruction if args.destroy else evaluate)(guest, report['samples'], recipe))
    except BaseException as exc:
        report['status'] = 'CHECK_FAILED'
        report['error'] = type(exc).__name__ + ': ' + str(exc)
        raise
    finally:
        native.SessionLock = old_lock_factory
        errors = []
        try:
            if staged:
                for name, data in original.items():
                    try:
                        if data is None:
                            (DISC / name).unlink(missing_ok=True)
                        else:
                            (DISC / name).write_bytes(data)
                    except OSError as exc:
                        errors.append(name + ': ' + str(exc))
            if staged or iso_moved:
                try:
                    ISO.unlink(missing_ok=True)
                    if iso_moved:
                        (folder / 'original-disc.iso').rename(ISO)
                except OSError as exc:
                    errors.append('ISO: ' + str(exc))
            if original is not None:
                report['disc_flags_restored'] = all(
                    ((DISC/name).read_bytes() if (DISC/name).exists() else None) == data for name,data in original.items())
            if 'exact_inputs' in report:
                pins = report['exact_inputs']
                report['exact_xbe_map_unchanged'] = sha256(XBE) == pins['xbe'] and sha256(MAP) == pins['map']
                report['original_iso_restored'] = (sha256(ISO) if ISO.exists() else None) == pins['iso']
            report['restoration_errors'] = errors
            if errors or any(report.get(name) is False for name in
                            ('disc_flags_restored', 'exact_xbe_map_unchanged', 'original_iso_restored')):
                report['status'] = 'CHECK_FAILED'
            if folder.exists():
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != ('PASS_OWNER_DESTRUCTION_RELEASE' if args.destroy else 'PASS_OWNER_QUALIFIED_CONTACT'):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
