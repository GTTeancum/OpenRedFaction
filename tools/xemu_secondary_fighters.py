"""Bounded stock64MiB two-Fighter motion, selective freeze and fresh restore.

Original hidden/unoccupied L20S2 Fighters4801 and18353 retain all entity bytes.
Original UnHide18456 links both and original fixed-point Goto18457. An isolated
ordinary Delay graph starts that chain, freezes trailing18353 at0.75 seconds,
and sends normal OFF through Invert at three seconds. Save120 frames with A
moving/B frozen; fresh-load120 frames proves exact state and resumed motion.
Original geometry/navigation and real two-sphere hulls remain intact. No host
input, images, NPCs, campaign playthrough, selected host or autonomous weapons.
A third short boot reads the genuine accepted RFSV1 APC save unchanged.

--prepare-only creates and independently decodes inputs. Parent owns all builds,
emulator runs, disc restoration and cleanup. This is a short first-leg check;
shared-goal pair blocking/arrival, formation, riders and firing are not claimed.
"""
import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct
import zipfile

from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import actor_details, event_rows, section_payload
from xemu_vehicle_fighter_switch import fighter_model
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd
from xemu_ai_projectile_save import fixture as storage_fixture
import xemu_secondary_apc as apc

A, B, SHOW, GOTO = 4801, 18353, 18456, 18457
START, FREEZE_TIMER, FREEZE, WAKE_TIMER, WAKE = range(916800, 916805)
EVENT_UIDS = (SHOW, GOTO, START, FREEZE_TIMER, FREEZE, WAKE_TIMER, WAKE)
NONE, HIDDEN, TIMER_PERIOD = 0xffffffff, 0x4000, 1072800000
CONFIG = 'campaign-secondary-fighters.bin'
FRAMES, LEGACY_FRAMES, ROW_WORDS = 120, 60, 384
SAMPLE_FRAMES = (0, 20, 40, 58, 60, 78, 100, 118, 119)
GOAL = [370.5423583984375, 13.248747825622559, -400.2716979980469]
OBSERVATION = 'rf_scene_secondary_fighter_samples'
SYMBOLS = dict(apc.SYMBOLS, **{OBSERVATION: len(SAMPLE_FRAMES) * ROW_WORDS,
    'rf_scene_secondary_fighter_pose_history': FRAMES * 24,
    'rf_scene_secondary_fighter_gap_frames': FRAMES, 'rf_scene_secondary_fighter_gap': 64,
    'rf_scene_secondary_fighter_fixture': 8, 'rf_scene_secondary_fighter_event_audit': 24,
    'rf_scene_secondary_fighter_edge_audit': 16, 'rf_scene_secondary_vehicle_edges': 10, 'rf_scene_secondary_fighter_clocks': len(SAMPLE_FRAMES),
    'rf_scene_vehicle_physics': 16, 'rf_scene_vehicle_physics_apply': 24,
    'rf_scene_vehicle_physics_checkpoint': 8, 'rf_scene_vehicle_physics_restore': 24,
    'rf_scene_world_controller_steps': 8, 'rf_scene_secondary_vehicle_pool_restore': 160})
LEGACY_HASH = '026277df463ca88cd032f38c81bfed2c91e60d299fd1375ba5dfe532bb9a2324'
LEGACY_ZIP = Path('/workspace/scratch/780c66634d06/OpenRedFaction-secondary-apc-checkpoint.zip')
LEGACY_PREFIX = 'evidence/xemu/secondary-apc-20261007-232039/'
LIMITATIONS = ('Two unoccupied original Fighters on a short fixed-point first leg; no selected host, '
    'riders, autonomous weapons, formations, full arrival or campaign progression. Shared-goal '
    'eventual pair blocking and static obstacle avoidance are outside this claim. Legacy APC continuation is one short fresh boot.')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def replay(frames=FRAMES):
    return b'RFI6' + U(48) + struct.pack('<5f7I', *([0] * 12)) * frames


def prepare_level(folder):
    folder.mkdir(parents=True, exist_ok=True)
    source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L20S2.rfl')
    meta = inspect(io.BytesIO(source), dict(offset=0, size=len(source), name='L20S2.rfl'))
    require(meta['version'] == 180 and not meta['trailing_bytes'], 'Incomplete original L20S2')
    entities = {row['uid']: row for row in entity_rows(source)}
    owners = [entities[uid] for uid in (A, B)]
    details = {str(row['uid']): actor_details(row) for row in owners}
    require(all(row['name'] == 'Fighter01' for row in owners) and
        all(v['health'] == 900. and v['armor'] == 0. and v['creation_flags'] == 2 and
            v['seat_host_uid'] == -1 for v in details.values()), 'Original Fighter identity/admission changed')
    original = {r['uid']: r for r in event_rows(section_payload(source, meta, 0x600))}
    require(original[SHOW]['type'] == 'UnHide' and original[SHOW]['links'] == [A, B, GOTO] and
        original[GOTO]['type'] == 'Goto' and original[GOTO]['links'] == [A, B] and
        list(struct.unpack_from('<3f', original[GOTO]['raw'], 10)) == GOAL,
        'Original UnHide or fixed-point Goto changed')
    events = [original[uid]['raw'] for uid in (SHOW, GOTO)] + [
        command(START, 'Delay', 'start_two_original_fighters', (SHOW, FREEZE_TIMER, WAKE_TIMER)),
        command(FREEZE_TIMER, 'Delay', 'freeze_trailing_fighter', (FREEZE,), delay=.75),
        event(FREEZE, 'Turn_Off_Physics', 'freeze_original_18353', (B,)),
        command(WAKE_TIMER, 'Delay', 'wake_saved_trailing_fighter', (WAKE,), delay=3.),
        event(WAKE, 'Invert', 'normal_off_to_saved_freeze', (FREEZE,))]
    replacements = {0x30000: U(2) + b''.join(r['raw'] for r in owners),
        0x600: U(len(events)) + b''.join(events), 0x3000: U(0), 0x40000: U(0),
        0x50000: U(0), 0x60000: U(0)}
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
        'Fixture section/header roundtrip failed')
    require([r['raw'] for r in entity_rows(out)] == [r['raw'] for r in owners], 'Original owner records changed')
    decoded = event_rows(section_payload(out, checked, 0x600))
    require([r['raw'] for r in decoded] == [bytes(r) for r in events] and
        [r['uid'] for r in decoded] == list(EVENT_UIDS), 'Fixture event roundtrip failed')
    for kind, digest in preserved.items():
        require(sha(section_payload(out, checked, int(kind, 16))) == digest,
            'Preserved original section changed: ' + kind)
    model, owner_evidence = fighter_model(), []
    for r in owners:
        pose = struct.unpack_from('<12f', r['raw'], r['transform'])
        # RFL disk basis is forward/right/up; core/level.c rotates rows to right/up/forward.
        basis = pose[6:12] + pose[3:6]
        spheres = [dict(center=[pose[k] + sum(s['center'][j] * basis[j*3+k] for j in range(3))
                    for k in range(3)], radius=s['radius']) for s in model['spheres']]
        owner_evidence.append(dict(uid=r['uid'], sha256=sha(r['raw']), position=list(pose[:3]),
            basis=list(basis), world_spheres=spheres))
    gap = min(math.dist(a['center'], b['center']) - a['radius'] - b['radius']
        for a in owner_evidence[0]['world_spheres'] for b in owner_evidence[1]['world_spheres'])
    require(.37 < gap < .38, 'Original independent sphere separation changed: ' + str(gap))
    path = folder / 'scene-fixture.vpp'
    archive(path, [('L20S2.rfl', out)])
    require(read_entry(path, 'L20S2.rfl') == out, 'Archive payload roundtrip failed')
    recipe = dict(scope=__doc__, source='levels2.vpp/L20S2.rfl', source_sha256=sha(source),
        fixture_sha256=sha(out), archive_sha256=sha(path.read_bytes()), preserved_sections=preserved,
        details=details, owners=owner_evidence, model=model, minimum_initial_sphere_gap=gap,
        events=[dict(uid=r['uid'], type=r['type'], name=r['name'], links=r['links'], sha256=sha(r['raw'])) for r in decoded],
        expected_goal=GOAL, source_frames=FRAMES, loaded_frames=FRAMES, freeze_seconds=.75, wake_seconds=3.,
        original_event_sha256={str(uid): sha(original[uid]['raw']) for uid in (SHOW, GOTO)},
        observation_frames=SAMPLE_FRAMES, staged_fields=['filter entities to original4801 and18353',
        'isolated ordinary Delay/Turn_Off_Physics/Invert graph', 'remove unrelated controllers/triggers/items/clutter'],
        limitations=LIMITATIONS)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    (folder / 'source-replay.bin').write_bytes(replay())
    (folder / 'loaded-replay.bin').write_bytes(replay())
    return path, recipe


def phase_inputs(level, phase):
    inputs = {'scene-fixture.vpp': level.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L20S2.rfl'.ljust(64, b'\0'),
        'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
        CONFIG: U(0x52544653, phase, 0, 0), 'player-replay.bin': replay()}
    if phase == 1:
        inputs.update({'campaign-setup.bin': U(START), 'world-hdd-save.flag': b'1'})
    else:
        inputs['world-hdd-load.flag'] = b'1'
    return inputs


def observed(guest, frame):
    at = SAMPLE_FRAMES.index(frame) * ROW_WORDS
    row = guest['extra'][OBSERVATION][at:at + ROW_WORDS]
    require(len(row) == ROW_WORDS and row[:2] == [1, frame] and not row[3],
        f'Missing/error exact native sample{frame}: {row[:4]}')
    return row


def owners(row):
    return {uid: row[16 + i*128:144 + i*128] for i, uid in enumerate((A, B))}


def event_observations(row):
    return {row[at]: row[at:at + 12] for at in range(272, 356, 12)}


def position(owner):
    return [f(v) for v in owner[28:31]]


def movement(a, b):
    return math.dist(position(a), position(b))


def describe(row):
    return dict(frame=row[1], owners={str(uid): dict(handle=v[1], slot=v[2], steps=v[8], last_frame=v[9],
        scripted=v[10], body_flags=v[11], position=position(v), rigid_velocity=[f(x) for x in v[40:43]],
        published_velocity=[f(x) for x in v[72:75]], route=dict(active=v[22], event=v[23],
        index=v[24], count=v[26], target=[f(x) for x in v[52:55]])) for uid, v in owners(row).items()},
        events={str(uid): dict(type=v[2], remaining_ms=-1 if v[4] == NONE else v[4],
            actor=v[5], source=v[6], flags=v[7], mode=v[8]) for uid, v in event_observations(row).items()},
        physics=row[356:372], total_steps=row[14])


def check_identity(row):
    require(row[4:8] == [2, 2, 2, 0] and row[9:12] == [0, 2, 7] and row[13] == NONE and not row[15],
        'Pool/selected/player/NPC ownership changed')
    handles = [v[1] for v in owners(row).values()] + [row[12]]
    require(len(set(handles)) == 3 and all(h not in (0, NONE) and h >> 16 for h in handles),
        'Missing/distinct full owner and player handles')
    for slot, (uid, v) in enumerate(owners(row).items()):
        wire = v[16:80]
        require(v[:8] == [uid, handles[slot], slot, 5, 11, 1, 1, 1] and v[12:16] == [2, 0, 0, NONE] and
            v[118:124] == [0, 0, NONE, NONE, 1, 1] and v[126:128] == [1, 1],
            'Fighter runtime lost independent published owner/real hull/empty-seat identity')
        require(wire[:3] == [uid, 5, 0] and wire[6:11] == [1, GOTO, 1, 0, 0] and
            wire[36:39] == list(struct.unpack('<3I', struct.pack('<3f', *GOAL))) and
            not any(wire[39:50]), 'Original fixed-point route changed or acquired graph nodes')
        require(wire[12:30] == v[80:98] and wire[30:36] == v[107:113] and
            f(v[114]) == f(wire[50]) == 900. and f(v[115]) == f(wire[51]) == 0. and
            v[116] == wire[52] and not v[116] & (2 | HIDDEN) and
            v[10:12] == wire[3:5] and v[124:126] == wire[3:5],
            'Actual rigid/publication/vitals/freeze marker differ from exact wire state')
        require(all(math.isfinite(f(x)) for x in wire[12:45] + wire[49:52] + wire[56:59] + v[80:113]),
            'Nonfinite state')
    require(row[14] == sum(v[8] for v in owners(row).values()), 'Aggregate and per-owner world steps disagree')
    require(event_observations(row)[GOTO][2] == 5, 'Display name was mistaken for Goto_Player type')


def check_steps(rows):
    ordered = sorted(rows)
    for lo, hi in zip(ordered, ordered[1:]):
        for uid in (A, B):
            a, b = owners(rows[lo])[uid], owners(rows[hi])[uid]
            require(0 <= b[8]-a[8] <= hi-lo, 'More than one committed owner step per frame')
            require(b[9] == min(hi, FRAMES-2) and (lo == 0 or a[9] == min(lo, FRAMES-2)),
                'Owner frame stamp differs from world frame')
    require(owners(rows[119]) == owners(rows[118]), 'Presentation-only endpoint stepped or changed an owner')
    for uid in (A, B):
        require(owners(rows[40])[uid][8] - owners(rows[20])[uid][8] == 20,
            'Both independent owners did not step once each during simultaneous motion')


def healthy(guest, phase, payload=None):
    require(guest['guest_phase'] == 5 and guest['frames'] == FRAMES and
        guest['memory_bytes'] == 64*1024*1024 and guest['free_pages'] > 0 and not guest['player_life'][2],
        'Incomplete living stock64MiB phase')
    x, state = guest['extra'], guest['checkpoint_state']
    require(x['rf_scene_secondary_fighter_fixture'] == [1, phase, len(SAMPLE_FRAMES), A, B, 1, int(phase == 1), 0],
        'Native fixture opt-in/sample/admission failure')
    require(not x['rf_scene_secondary_vehicle'][13] and not x['rf_scene_vehicle_state'][5] and
        not any(x['rf_scene_vehicle_damage'][i] for i in (2, 3)) and
        not any(x['rf_scene_passive_damage'][i] for i in (2, 3)) and
        not any(x['rf_scene_enemy_combat'][i] for i in (2, 7)) and not any(x['rf_scene_script_slays']) and
        not any(guest['level_transitions']) and not any(guest['level_request']) and
        not x['rf_scene_secondary_vehicle_edges'][8],
        'Unexpected damage, combat, secondary error or campaign transition')
    world = x['rf_scene_world_controller_steps']
    require(world[0] == world[1] and not world[6] and not world[7], 'Duplicate/error world-controller step')
    if phase == 1:
        require(x['rf_scene_setup_result'] == [1, START, 48, 0] and payload is not None and
            state[9] == 1 and not state[3] and state[4] == len(payload), 'Ordinary setup/save failed')
    else:
        require(state[8] == 1 and not state[0] and not state[9] and
            not any(x['rf_scene_setup_result']) and not any(x['rf_scene_world_load_reject']),
            'Fresh load failed, saved again or replayed setup')


def saved_rows(payload, final):
    sections = checkpoint_sections(payload)
    data = sections[11]
    require(data[:4] == b'RFPV', 'Selective freeze omitted RFPV')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require((version, count) == (1, 2) and len(data) == 16 + inner + count*72,
        'Expected RFPV1 with two passive owners and no selected hidden host')
    physics = [list(struct.unpack_from('<18I', data, 16 + inner + i*72)) for i in range(count)]
    data = data[16:16+inner]
    require(data[:4] == b'RFSV' and struct.unpack_from('<I', data, 4)[0] == 2, 'Expected genuine RFSV2')
    version, inner, count = struct.unpack_from('<3I', data, 4)
    require(count == 2 and len(data) == 16 + inner + count*256, 'RFSV2 row count/size changed')
    wires = [list(struct.unpack_from('<64I', data, 16+inner+i*256)) for i in range(count)]
    data = data[16:16+inner]
    require(data[:4] == b'RFVA' and struct.unpack_from('<3I', data, 4) == (2, 0, 2) and len(data) == 176,
        'RFVA2 gained a selected host or lost a passive owner')
    passive = [list(struct.unpack_from('<20I', data, 16+i*80)) for i in range(2)]
    for i, (uid, v) in enumerate(owners(final).items()):
        wire, phy, published = wires[i], physics[i], passive[i]
        require(wire == v[16:80] and published[:2] == [uid, 0] and published[2:14] == wire[12:24] and
            published[14:20] == wire[50:56], 'Ordinary saved owner differs from final world observation')
        require(phy[:4] == [uid, 1, wire[3], wire[4]] and phy[8:11] == wire[56:59] and
            phy[5] == wire[52] & 0x06000000 and not phy[4] and not any(phy[6:8]) and not any(phy[11:]),
            'Saved RFPV1 disagrees with independent owner frozen state')
    native, saved_events = event_observations(final), {}
    require(len(sections[4]) == len(EVENT_UIDS)*192, 'Saved event count changed')
    for at in range(0, len(sections[4]), 192):
        raw = sections[4][at:at+192]
        require(raw[:4] == b'RFEC' and struct.unpack_from('<2I', raw, 4) == (3, 192), 'Expected RFEC3')
        apc.check_checksum(raw, 'RFEC3')
        words = list(struct.unpack('<48I', raw)); uid = words[4]
        require(uid in native and str(uid) not in saved_events, 'Unexpected duplicate saved event')
        row = native[uid].copy()
        require(row[2] == words[5] and row[7:9] == words[16:18] and not words[18] and not words[28],
            'Saved ordinary event state changed')
        row[4] = words[40]
        if row[2] == 50:
            row[9:12] = [words[39], words[41], words[42]]
        saved_events[str(uid)] = row
    return dict(bytes=len(payload), sha256=sha(payload), wires=wires, physics=physics, passive=passive,
        full_rigid=[v[80:114] for v in owners(final).values()], handles=[v[1] for v in owners(final).values()] + [final[12]],
        events=saved_events, formats=['RFWC', 'RFPV1', 'RFSV2', 'RFVA2(host_bytes=0)', 'RFEC3'])


def check_loaded_exact(guest, saved):
    first = observed(guest, 0)
    require(guest['checkpoint_state'][1] == saved['bytes'], 'Fresh load read unexpected bytes')
    require([v[16:80] for v in owners(first).values()] == saved['wires'] and
        [v[80:114] for v in owners(first).values()] == saved['full_rigid'] and
        [v[1] for v in owners(first).values()] + [first[12]] == saved['handles'],
        'Fresh frame0 differs from exact independent saved rigid/route/owner/frozen state')
    restore = guest['extra']['rf_scene_secondary_vehicle_pool_restore']
    for i, (uid, v) in enumerate(owners(first).items()):
        assigned = restore[i*80:(i+1)*80]
        require(assigned[:8] == [1, uid, v[1], v[2], 5, 0, GOTO, 1] and
            assigned[8:72] == saved['wires'][i] and assigned[72:75] == [2, 2, 2],
            'Immutable assignment differs from exact saved owner row')
    require(not any(first[356:372]), 'Restore replayed freeze or wake callback')
    clock = guest['extra']['rf_scene_secondary_fighter_clocks'][0]
    require(0 <= clock <= TIMER_PERIOD, 'Invalid actual timer query clock')
    expected = {}
    for uid, values in saved['events'].items():
        row = values.copy()
        for at in (4, 9):
            if row[at] == NONE:
                continue
            delta = row[at] - clock
            if delta > TIMER_PERIOD//2:
                delta -= TIMER_PERIOD
            elif delta < -TIMER_PERIOD//2:
                delta += TIMER_PERIOD
            row[at] = delta & NONE
        expected[int(uid)] = row
    require(event_observations(first) == expected, 'Fresh exact event state or clock-adjusted timers changed')


def validate_gap(guest, model=None):
    """Decode every native interval and independently reconstruct the worst pair."""
    model = model or fighter_model()
    summary = guest['extra']['rf_scene_secondary_fighter_gap']
    words = guest['extra']['rf_scene_secondary_fighter_gap_frames']
    gaps = [f(v) for v in words]
    first = owners(observed(guest, 0))
    require(summary[:3] == [FRAMES, FRAMES-1, 0] and len(gaps) == FRAMES and
        summary[5:7] == [first[A][1], first[B][1]] and summary[9:11] == [2, 2] and
        summary[54:56] == [A, B], 'Missing per-frame actual pair-hull gap coverage or identity')
    require(all(math.isfinite(v) and v > 0 for v in gaps) and summary[11:13] == [0, NONE],
        'Real pair hull touched/overlapped during bounded independence proof: ' +
        str(dict(minimum=min(gaps), frame=summary[3], nonpositive_frames=summary[11])))
    require(summary[3] == gaps.index(min(gaps)) and summary[4] == words[summary[3]] and words[119] == words[118],
        'Native global minimum or presentation-only endpoint gap changed')
    poses = [[f(v) for v in summary[at:at+12]] for at in (16, 28)]
    local = [[f(v) for v in summary[at:at+4]] for at in (40, 44)]
    logged_world = [[f(v) for v in summary[at:at+3]] for at in (48, 51)]
    require(all(math.isfinite(v) for pose in poses for v in pose), 'Nonfinite recorded minimum owner pose')
    for i, index in enumerate(summary[7:9]):
        require(0 <= index < 2 and local[i] == model['spheres'][index]['center'] + [model['spheres'][index]['radius']],
            'Recorded winning sphere differs from original actual Fighter collision resource')

    def float32(value):
        return struct.unpack('<f', struct.pack('<f', value))[0]

    def transform(pose, sphere):
        result = []
        for axis in range(3):
            value = pose[axis]
            for direction in range(3):
                value = float32(value + float32(sphere['center'][direction] * pose[3+direction*3+axis]))
            result.append(value)
        return result

    worlds = [[transform(pose, sphere) for sphere in model['spheres']] for pose in poses]
    pairs = [(math.dist(worlds[0][i], worlds[1][j]) - model['spheres'][i]['radius'] - model['spheres'][j]['radius'], i, j)
        for i in range(2) for j in range(2)]
    independently_smallest = min(pairs)
    require(list(independently_smallest[1:]) == summary[7:9] and
        abs(independently_smallest[0] - min(gaps)) < .0001 and
        all(math.dist(worlds[i][summary[7+i]], logged_world[i]) < .0001 for i in range(2)),
        'Independent actual-sphere/pose reconstruction disagrees with native minimum')
    require(abs(math.dist(*logged_world)-local[0][3]-local[1][3]-min(gaps)) < .00001,
        'Recorded minimum gap disagrees with recorded actual world sphere centers')
    return dict(result='PASS', observed_frames=summary[0], minimum_gap=min(gaps), minimum_frame=summary[3],
        sphere_indices=summary[7:9], full_handles=summary[5:7], owner_poses=poses,
        local_spheres=local, world_centers=logged_world, recomputed_gap=independently_smallest[0],
        original_model_sha256=model['sha256'], nonpositive_frames=summary[11], per_frame_minimum_gap=gaps)


def validate_source(guest, payload, recipe):
    healthy(guest, 1, payload)
    gap_evidence = validate_gap(guest, recipe['model'])
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES}
    for row in rows.values():
        check_identity(row)
    require(len({tuple(v[1] for v in owners(row).values()) + (row[12],) for row in rows.values()}) == 1,
        'Source changed full owner/player identities')
    check_steps(rows)
    for uid in (A, B):
        require(movement(owners(rows[20])[uid], owners(rows[40])[uid]) > .1 and
            math.dist(position(owners(rows[40])[uid]), GOAL) < math.dist(position(owners(rows[20])[uid]), GOAL),
            'Both original Fighters did not simultaneously move toward the authored fixed point')
    frozen, last = owners(rows[60]), owners(rows[118])
    require(frozen[B][10] == last[B][10] == 1 and frozen[B][11] & 0x98000000 == 0x18000000 and
        frozen[B][16:114] == last[B][16:114] and frozen[B][8] == last[B][8] and
        not any(last[B][40:46]) and not any(last[B][72:75]), 'Trailing owner did not remain selectively frozen')
    require(movement(frozen[A], owners(rows[78])[A]) > .1 and last[A][8]-frozen[A][8] == 58,
        'Leading owner did not advance independently before static blocking')
    require(guest['extra']['rf_scene_vehicle_physics'][:3] == [1, 1, 0], 'Expected exactly one ordinary freeze')
    audit = guest['extra']['rf_scene_secondary_vehicle_checkpoint_audit']
    require(audit[0] == 1 and audit[1] == 20 and audit[2] == (1 << audit[1])-1 and not audit[3] and not audit[4] and audit[7] == 1,
        'Multi-owner native malformed/no-mutation admission audit failed: ' + str(audit))
    event_audit = guest['extra']['rf_scene_secondary_fighter_event_audit']
    require(event_audit[:16] == [1, 100, 15, 0x7fff, 0, 0, 0xfffffffc, 64, 3, 2, 1, 3, 2, 1, 3, 16] and
        event_audit[16:22] == [7, 8, NONE, NONE, 17, 18] and 0 < event_audit[22] < 32768 and event_audit[23] == 1,
        'Isolated real-dispatch UnHide/Goto/delay/OFF/cooldown/cycle regression failed: ' + str(event_audit))
    edge_audit = guest['extra']['rf_scene_secondary_fighter_edge_audit']
    require(edge_audit[:6] == [12, 12, 0, 0, 0, 1] and
        abs(f(edge_audit[6]) - .377991527) < 1e-6 and abs(f(edge_audit[7]) - .4) < 1e-6 and
        edge_audit[8:13] == [1, 0xfffffffc, 1, 1, 73] and edge_audit[13:] == [0, 1, 31],
        'Native independent-Fighter static-edge regression failed: ' + str(edge_audit))
    edge_contacts = guest['extra']['rf_scene_secondary_vehicle_edges']
    require(edge_contacts[0] >= edge_contacts[1] > 0 and edge_contacts[2] in (A, B) and
        edge_contacts[3] == owners(rows[118])[edge_contacts[2]][1] and edge_contacts[6] < 2 and
        math.isfinite(f(edge_contacts[7])) and 0 <= f(edge_contacts[7]) <= 1 and not edge_contacts[8],
        'Actual independent Fighter static-edge complement was not exercised')
    saved = saved_rows(payload, rows[118])
    require(saved['events'][str(FREEZE_TIMER)][4] == NONE and saved['events'][str(WAKE_TIMER)][4] not in (0, NONE),
        'Save lost ordinary pending wake timer')
    return dict(result='PASS', saved=saved, malformed_audit=audit, event_dispatch_audit=event_audit,
        actual_edge_contacts=edge_contacts, static_edge_audit=edge_audit,
        initial_sphere_gap=recipe['minimum_initial_sphere_gap'], actual_pair_gap=gap_evidence, observations={str(k): describe(v) for k, v in rows.items()})


def validate_loaded(guest, saved):
    healthy(guest, 2)
    gap_evidence = validate_gap(guest)
    check_loaded_exact(guest, saved)
    require(not any(guest['extra']['rf_scene_secondary_fighter_event_audit']) and
        not any(guest['extra']['rf_scene_secondary_fighter_edge_audit']), 'Fresh load repeated source-only audits')
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES}
    for row in rows.values():
        check_identity(row)
    require(all([v[1] for v in owners(row).values()] + [row[12]] == saved['handles'] for row in rows.values()),
        'Fresh continuation changed full owner/player identities')
    for lo, hi in zip(SAMPLE_FRAMES, SAMPLE_FRAMES[1:]):
        for uid in (A, B):
            a, b = owners(rows[lo])[uid], owners(rows[hi])[uid]
            require(0 <= b[8]-a[8] <= hi-lo and b[9] == min(hi, FRAMES-2),
                'Fresh owner stepped more than once per frame')
    require(owners(rows[119]) == owners(rows[118]), 'Fresh presentation-only endpoint changed an owner')
    require(owners(rows[40])[B][16:114] == owners(rows[0])[B][16:114] and
        owners(rows[40])[B][8] == 0 and movement(owners(rows[0])[A], owners(rows[40])[A]) > .1,
        'Fresh continuation did not preserve selective freeze while other Fighter moved')
    for uid in (A, B):
        lo, hi = owners(rows[78])[uid], owners(rows[118])[uid]
        require(hi[8]-lo[8] == 40 and hi[22:24] == [1, GOTO],
            'Retained owners lost independent stepping or route state')
    require(movement(owners(rows[78])[B], owners(rows[118])[B]) > .1,
        'Trailing owner did not actually move after ordinary wake')
    stopped = owners(rows[40])[A][28:40]
    history = guest['extra']['rf_scene_secondary_fighter_pose_history']
    require(len(history) == FRAMES*24 and
        all(history[frame*24:frame*24+12] == stopped for frame in range(40, FRAMES)) and
        all(not any(owners(rows[frame])[A][72:75]) for frame in (58, 60, 78, 100, 118, 119)),
        'Leading owner did not retain its admitted static-contact pose')
    edge_contacts = guest['extra']['rf_scene_secondary_vehicle_edges']
    require(edge_contacts[0] >= edge_contacts[1] > 0 and
        edge_contacts[2:7] == [A, owners(rows[118])[A][1], 16, 918, 0] and
        f(edge_contacts[7]) == 0 and not edge_contacts[8],
        'Leading stop lacks the exact original-ramp zero-time contact')
    p = guest['extra']['rf_scene_vehicle_physics']
    require(p[:3] == [1, 0, 1] and p[8] == B and p[10] == 1 and
        owners(rows[118])[B][11] & 0x80000000 and owners(rows[118])[B][10] == 1,
        'Fresh ordinary OFF did not wake only the trailing owner')
    return dict(result='PASS', source_payload_sha256=saved['sha256'], actual_pair_gap=gap_evidence,
        leading_post_load_movement=movement(owners(rows[0])[A], owners(rows[40])[A]),
        trailing_post_wake_movement=movement(owners(rows[78])[B], owners(rows[118])[B]),
        leading_static_stop_from_frame=40, actual_edge_contacts=edge_contacts,
        observations={str(k): describe(v) for k, v in rows.items()})


def prepare_legacy(folder, bundle):
    folder.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(bundle) as z:
        payload = z.read(LEGACY_PREFIX + 'source/xbox-world.rfwc')
        level = z.read(LEGACY_PREFIX + 'level/scene-fixture.vpp')
        old = json.loads(z.read(LEGACY_PREFIX + 'validated-report.json'))
    require(len(payload) == 4912 and sha(payload) == LEGACY_HASH and old['result'] == 'PASS',
        'Accepted original RFSV1 legacy evidence changed')
    local = ROOT / 'artifacts/xemu/secondary-apc-20261007-232039/source/xbox-world.rfwc'
    if local.exists():
        require(local.read_bytes() == payload, 'Local and archived accepted APC payloads disagree')
    require(sha(level) == old['recipe']['archive_sha256'] and
        old['validation']['source']['saved']['sha256'] == LEGACY_HASH, 'Legacy archive/save provenance changed')
    # This transport envelope is the existing helper. Payload and source RFL
    # remain byte-identical; no row rewrite or replacement codec is permitted.
    (folder / 'scene-fixture.vpp').write_bytes(level)
    (folder / 'xbox-world.rfwc').write_bytes(payload)
    (folder / 'legacy-replay.bin').write_bytes(replay(LEGACY_FRAMES))
    evidence = dict(payload_bytes=len(payload), payload_sha256=sha(payload), archive_sha256=sha(level),
        source_bundle=str(bundle), source_entry=LEGACY_PREFIX + 'source/xbox-world.rfwc',
        transport='Unchanged RFWC payload in existing RFSG1 world-fixture.0 read-only loader')
    (folder / 'recipe.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return folder / 'scene-fixture.vpp', payload, old['validation']['source']['saved'], evidence


def validate_legacy(guest, saved):
    require(guest['guest_phase'] == 5 and guest['frames'] == LEGACY_FRAMES and
        guest['memory_bytes'] == 64*1024*1024 and guest['free_pages'] > 0 and not guest['player_life'][2],
        'Incomplete genuine legacy stock64MiB fresh boot')
    state, x = guest['checkpoint_state'], guest['extra']
    require(state[8] == 1 and not state[0] and state[1] == 4912 and not state[9] and
        not any(x['rf_scene_setup_result']) and not any(x['rf_scene_world_load_reject']), 'Genuine RFSV1 load failed')
    apc.check_loaded_exact(guest, saved)
    rows = {frame: apc.observed(guest, frame) for frame in (0, 20, 40)}
    for row in rows.values():
        apc.check_identity(row, False)
    require(apc.movement(rows[0], rows[40]) > .1 and rows[40][28] != rows[0][28] and
        not x['rf_scene_secondary_vehicle'][9] and not x['rf_scene_secondary_vehicle'][13] and
        not any(guest['level_transitions']) and not any(guest['level_request']),
        'Genuine old APC did not continue real retained motion before retirement')
    return dict(result='PASS', payload_sha256=LEGACY_HASH, unchanged_real_RFSV1=True,
        observations={str(k): apc.describe(v) for k, v in rows.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--run-root', type=Path)
    parser.add_argument('--legacy-bundle', type=Path, default=LEGACY_ZIP)
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('Timeout must be positive')
    folder = args.prepare_only or args.run_root or ROOT / 'artifacts/xemu' / (
        'secondary-fighters-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True, exist_ok=args.prepare_only is not None)
    level, recipe = prepare_level(folder / 'level')
    legacy_level, legacy_payload, legacy_saved, legacy = prepare_legacy(folder / 'legacy-level', args.legacy_bundle)
    if args.prepare_only:
        print(json.dumps(dict(result='PREPARED_NOT_RUN', path=str(level), recipe=recipe, legacy=legacy), indent=2))
        return
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    hdd = prepare_hdd(ROOT, base)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | \
        {'scene-preview.flag', 'player-control.flag', 'scene-fixture.vpp', CONFIG, apc.CONFIG}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
    report = dict(result='FAIL', scope=__doc__, recipe=recipe, legacy=legacy, phases={}, validation={}, binaries={},
        input_sha256={}, original_input_sha256={name: sha(data) if data is not None else None for name, data in original.items()},
        limitations=LIMITATIONS, hdd=str(hdd), source_persisted_to_isolated_test_hdd=True)
    restore = folder / 'input-restore'; restore.mkdir()
    for name, data in original.items():
        if data is not None:
            (restore / name).write_bytes(data)
    (restore / 'manifest.json').write_text(json.dumps(report['original_input_sha256'], indent=2) + '\n')

    def write_report():
        (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    def stage(label, inputs):
        require('campaign-setup-immediate.flag' not in inputs, 'Delay setup must use ordinary dispatch')
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        for name, data in inputs.items():
            require(name in names, 'Untracked disc input ' + name)
            (DISC / name).write_bytes(data)
        report['input_sha256'][label] = {name: sha(data) for name, data in inputs.items()}
        write_report(); build(folder, label)
        report['binaries'][label] = preserve_launch_binaries(folder / (label + '-binaries'))
        write_report()

    try:
        saved = None
        for phase, label in ((1, 'source'), (2, 'loaded')):
            stage(label, phase_inputs(level, phase))
            guest = run_guest(folder, label, hdd, FRAMES, args.seconds, snapshot=phase == 2,
                capture_world=phase == 1, extra_symbols=SYMBOLS, allow_guest_error=True)
            report['phases'][label] = guest; write_report()
            if phase == 1:
                payload = (folder / label / 'xbox-world.rfwc').read_bytes()
                report['validation'][label] = validate_source(guest, payload, recipe)
                saved = report['validation'][label]['saved']
            else:
                report['validation'][label] = validate_loaded(guest, saved)
            write_report()
        inputs = {'scene-fixture.vpp': legacy_level.read_bytes(),
            'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'L1S3.rfl'.ljust(64, b'\0'),
            'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
            apc.CONFIG: U(0x43504153, 2, 0, 0), 'player-replay.bin': replay(LEGACY_FRAMES),
            'world-fixture-load.flag': b'1', 'world-fixture.0': storage_fixture(legacy_payload)}
        stage('legacy', inputs)
        guest = run_guest(folder, 'legacy', base, LEGACY_FRAMES, args.seconds, snapshot=True,
            extra_symbols=SYMBOLS, allow_guest_error=True)
        report['phases']['legacy'] = guest; write_report()
        report['validation']['legacy'] = validate_legacy(guest, legacy_saved)
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
