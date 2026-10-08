"""Bounded Xbox living Jeep driver across parking, ordinary save and return.

Original miner7646 retains Jeep7629, authored health and its real driver tag.
CTF06 holds that stationary pair and empty Jeep copy915200. Process-contained
Use/fire/exit inputs spend independent ammo, park the living driver and save
occupied B. Two fresh boots read that same ordinary HDD save: exit B and
return as A's gunner, or remain in B until the saved Delay slays passive A and
the ordinary wreck callback safely releases its still-living original driver.
Native exact-frame observations are read-only. No images, host input, guest
memory writes, fabricated saves, AI routes, campaign traversal or PC runtime.

--prepare-only creates and independently decodes the fixture. The parent owns
all builds/emulator runs and cleanup. Failed phases and prelaunch binaries are
retained; every touched disc input, including scene-preview.flag, is restored.
"""

import argparse
import datetime
import io
import json
import math
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry, U, F
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_npc_jeep_seat import ACTOR, jeep_seat
from xemu_npc_turret_seat import record_details
from xemu_turret_combat import entity_rows
from xemu_vehicle_switch import A, B, prepare_level as prepare_pair
from xemu_vehicle_switch_save import SYMBOLS as SAVE_SYMBOLS
from xemu_vehicle_npc_push import event_rows, section_payload
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd

TIMER, SLAY, NONE = 916600, 916601, 0xffffffff
CONFIG = 'campaign-passive-jeep-driver.bin'
SOURCE_FRAMES, RETURN_FRAMES, WRECK_FRAMES = 350, 140, 320
SAMPLE_FRAMES = (0, 40, 80, 100, 118, 138, 190, 230, 248, 278, 310, 318, 348, 349)
ROW_WORDS = 152
# Source278..349: <=0.301mm vertical settling, no horizontal displacement,
# <=0.000189 basis-component change. The restored selected rigid body keeps
# simulating normally; only parked A has a bit-exact frozen pose contract.
B_SETTLING_LIMITS = dict(horizontal_m=.0001, vertical_m=.001, basis_component=.001)
OBSERVATION = 'rf_scene_passive_jeep_driver_samples'
SYMBOLS = dict(SAVE_SYMBOLS, **{OBSERVATION: len(SAMPLE_FRAMES) * ROW_WORDS},
    rf_scene_passive_jeep_driver_fixture=8, rf_scene_passive_jeep_restore_audit=4,
    rf_scene_passive_jeep_restore=8,
    rf_scene_script_slays=6, rf_scene_npc_seat_probe=12, rf_scene_npc_seat_checkpoint=6,
    rf_scene_vehicle_wreck_exit=12, rf_scene_vehicle_wreck_exit_apply=32,
    rf_scene_live_death_audio=4, rf_scene_jeep_npc_gunner=16)
LIMITATIONS = ('One stationary original living Jeep driver and one empty same-class Jeep; '
    'ordinary save and two fresh boots. No independently moving NPC vehicle AI, '
    'extra passengers, current-session restore, cross-class occupied transfer, '
    'visual/audio verification or campaign progression.')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def replay(phase=1):
    frames = {1: SOURCE_FRAMES, 2: RETURN_FRAMES, 3: WRECK_FRAMES}[phase]
    uses = (90, 210, 250) if phase == 1 else (60, 100) if phase == 2 else ()
    return b'RFI6' + U(48) + b''.join(struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0,
        int(frame in uses), int(phase == 1 and (130 <= frame < 150 or 290 <= frame < 298)),
        0, int(phase == 1 and frame == 270), 0) for frame in range(frames))


def prepare_level(folder):
    path, recipe = prepare_pair(folder)
    raw = read_entry(path, 'ctf06.rfl')
    source = read_entry(ROOT / 'Installed_Game/levels2.vpp', 'L12S1.rfl')
    miner = next(row for row in entity_rows(source) if row['uid'] == ACTOR)
    details, seat = record_details(miner), jeep_seat()
    require(miner['name'].lower() == 'miner1' and details['seat_host_uid'] == A,
            'Original living miner lost its authored Jeep7629 relation')
    actor = bytearray(miner['raw'])
    at, host = miner['transform'], recipe['positions'][0]
    actor[at:at + 48] = F(*(host[i] + seat['position'][i] for i in range(3)), 0, 0, 1, 1, 0, 0, 0, 1, 0)
    require(actor[:at] == miner['raw'][:at] and actor[at + 48:] == miner['raw'][at + 48:],
            'Miner edit exceeded the declared transform')
    records = [row['raw'] for row in entity_rows(raw)] + [bytes(actor)]
    # One real scheduled event. Its remaining deadline is written/read by RFEC3.
    events = U(2) + command(TIMER, 'Delay', 'passive_jeep_saved_wreck_timer', (SLAY,), delay=10.0) + \
        command(SLAY, 'Slay_Object', 'slay_original_parked_jeep', (A,))
    replacements = {0x30000: U(3) + b''.join(records), 0x600: events, 0x20000: U(0), 0x10000: U(0)}
    meta = inspect(io.BytesIO(raw), dict(offset=0, size=len(raw), name='ctf06.rfl'))
    out, offsets = bytearray(raw[:meta['sections'][0]['offset']]), {}
    for section in meta['sections']:
        kind = int(section['type'], 16)
        payload = replacements.get(kind, raw[section['offset'] + 8:section['offset'] + 8 + section['size']])
        offsets[kind] = len(out)
        out += U(kind, len(payload)) + payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    checked = inspect(io.BytesIO(out), dict(offset=0, size=len(out), name='ctf06.rfl'))
    require([row['raw'] for row in entity_rows(out)] == records, 'Entity records failed independent roundtrip')
    decoded_events = event_rows(section_payload(out, checked, 0x600))
    require([(r['uid'], r['type'], r['links']) for r in decoded_events] ==
            [(TIMER, 'Delay', [SLAY]), (SLAY, 'Slay_Object', [A])], 'Unexpected authored event dependency')
    archive(path, [('ctf06.rfl', out)])
    recipe.update(scope=__doc__, frames=SOURCE_FRAMES, return_frames=RETURN_FRAMES, wreck_frames=WRECK_FRAMES,
        seat=seat, driver=details, driver_source_sha256=sha(miner['raw']), driver_staged_sha256=sha(bytes(actor)),
        fixture_sha256=sha(bytes(out)), archive_sha256=sha(path.read_bytes()),
        events=[dict(uid=e['uid'], type=e['type'], links=e['links'], sha256=sha(e['raw'])) for e in decoded_events],
        staged_fields=['original A and miner transforms', 'B fixture UID915200 and transform',
                       'player spawn', 'Delay916600 at10 seconds linked to Slay_Object916601 for A'],
        replay={'board_gunner_A': 90, 'fire_A': [130, 149], 'exit_A': 210, 'board_B': 250,
                'gunner_B': 270, 'fire_B': [290, 297], 'save_B': SOURCE_FRAMES},
        return_replay={'exit_B': 60, 'return_gunner_A': 100, 'finish_before_wreck': RETURN_FRAMES},
        wreck_replay={'inputs': 'idle in restored B', 'saved_timer_expected_frame': 252, 'finish': WRECK_FRAMES},
        limitations=LIMITATIONS)
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    for phase, label in ((1, 'source'), (2, 'return'), (3, 'wreck')):
        (folder / (label + '-replay.bin')).write_bytes(replay(phase))
    return path, recipe


def observed(guest, frame):
    values = guest['extra'][OBSERVATION]
    at = SAMPLE_FRAMES.index(frame) * ROW_WORDS
    row = values[at:at + ROW_WORDS]
    require(row[:2] == [1, frame] and row[3] == 0, f'Missing/error exact-frame native sample{frame}: {row[:4]}')
    return row


def describe(row):
    return dict(frame=row[1], active_uid=row[4], handles=dict(active=row[5], player=row[6], npc=row[7], A=row[8], B=row[9]),
        attached=row[12], npc_parent=row[15], npc_mode=row[16], npc_health=f(row[18]), npc_armor=f(row[19]),
        A=dict(selected=row[20], kind=row[21], registry=row[22], entity_view=row[23], driver=row[24],
               seats=row[25:27], health=f(row[28]), destroyed=row[31], pose_bits=row[32:44], ammo=row[110], rng=row[111]),
        B=dict(selected=row[44], kind=row[45], registry=row[46], entity_view=row[47], driver=row[48],
               seats=row[49:51], health=f(row[52]), destroyed=row[55], pose_bits=row[56:68], ammo=row[113], rng=row[114]),
        npc_pose_bits=row[68:80], expected_seat_bits=row[80:92], seated_pose_exact=row[131],
        player_position=[f(v) for v in row[106:109]], player_occupied=row[100], player_role=row[103],
        timer_remaining_ms=row[119] if row[119] != NONE else -1, slays=row[124], wreck_exits=row[127],
        placement=dict(kind=row[135], status=row[136], clear=row[137], unchanged=row[149],
                       actual_npc_spheres=row[133], A_spheres=row[147], B_spheres=row[148],
                       gap_A=f(row[144]), gap_B=f(row[145])))


def check_guest(guest, phase, payload_bytes=0):
    require(guest['player_life'][2] == 0, 'Player died during the bounded continuation')
    frames = {1: SOURCE_FRAMES, 2: RETURN_FRAMES, 3: WRECK_FRAMES}[phase]
    require(guest['guest_phase'] == 5 and guest['frames'] == frames and
            guest['memory_bytes'] == 64 * 1024 * 1024 and guest['free_pages'] > 0, 'Incomplete stock64MiB phase')
    x, state = guest['extra'], guest['checkpoint_state']
    count = sum(frame < frames for frame in SAMPLE_FRAMES)
    require(x['rf_scene_passive_jeep_driver_fixture'] == [1, phase, count, A, B, ACTOR, 1, 0],
            'Native explicit opt-in fixture, scene identity or sample count failed')
    require(not x['rf_scene_vehicle_state'][5] and not x['rf_scene_npc_seats'][9] and
            not any(x['rf_scene_vehicle_route_state'][i] for i in (0, 3, 5, 7)) and
            not any(x['rf_scene_enemy_combat'][i] for i in (2, 7)), 'Unexpected route, seat error or NPC combat')
    if phase == 1:
        require(state[9] == 1 and state[3] == 0 and state[4] == payload_bytes, 'Ordinary source save failed')
        require(x['rf_scene_setup_result'] == [1, TIMER, 48, 0] and not any(x['rf_scene_passive_jeep_restore_audit']),
                'Source did not schedule exactly one genuine Delay')
    else:
        require(state[8] == 1 and state[0] == 0 and state[1] == payload_bytes and state[9] == 0 and
                not any(x['rf_scene_world_load_reject']) and not any(x['rf_scene_setup_result']),
                'Fresh load failed or replayed setup/saved unexpectedly')
        audit = x['rf_scene_passive_jeep_restore_audit']
        require(audit == [1, 22, 22, 0],
                f'Native malformed passive-seat admission check failed: {audit}')
        admission = x['rf_scene_passive_jeep_restore']
        require(admission[:3] == [1, 1, 1] and admission[3] >= 3 and admission[4:] == [A, ACTOR, 6, 0],
                f'Saved passive host actual-hull admission failed: {admission}')


def check_identity(row, recipe, attached=True):
    require(len(set(row[6:10])) == 4 and not any(v in (0, NONE) for v in row[6:10]) and row[10:12] == [1, 1],
            'Actor/player/host full handles are not distinct exact registered identities')
    require(row[13:15] == [1, recipe['seat']['index']] and f(row[18]) == recipe['driver']['authored_health'] and
            f(row[19]) == recipe['driver']['authored_armor'] and row[132] == 1 and row[133] > 0,
            'Original driver identity, authored vitals or actual published body was lost')
    for base, uid in ((20, A), (44, B)):
        selected = int(row[4] == uid)
        require(row[base] == selected and row[base + 2:base + 4] == [1, selected] and
                (selected or row[base + 1] == 11), 'Host registry pointer/kind/entity-view ownership mismatch')
    require(row[4] in (A, B) and row[5] == row[8 if row[4] == A else 9], 'Active host UID/handle mismatch')
    require(all(math.isfinite(f(v)) for v in row[18:20] + row[28:30] + row[32:44] +
                row[52:54] + row[56:98] + row[106:109]), 'Nonfinite live owner pose or health')
    if attached:
        require(row[12] == 1 and row[15:18] == [row[8], 13, 13] and row[24:26] == [row[7]] * 2 and
                row[99] == NONE and row[131] == 1 and row[68:80] == row[80:92] and
                row[95:99] == [0, 0, 0, 0], 'Living driver lost reciprocal A seat, mode13 or exact seat pose')
    else:
        require(row[12] == 0 and row[15] == NONE and row[16] != 13 and row[17] != 13 and
                row[24:27] == [NONE] * 3 and row[99] == NONE, 'Released living driver retained an ownership endpoint')


def check_player(row, host_uid=None):
    if host_uid is None:
        require(row[100] == 0 and row[104] == NONE, 'Player did not exit normally')
        return
    host = row[8 if host_uid == A else 9]
    require(row[4] == host_uid and row[100:105] == [1, host, row[6], 1, host],
            'Player was not the real gunner of the expected Jeep')
    expected = [row[7], row[7], row[6]] if host_uid == A else [row[6], row[6], NONE]
    start = 24 if host_uid == A else 48
    require(row[start:start + 3] == expected, 'Player stole or fabricated a driver/passenger endpoint')


def saved_rows(payload, recipe):
    sections = checkpoint_sections(payload)
    data = sections[11]
    require(data[:4] == b'RFSW' and len(data) >= 344, 'Missing ordinary switched-owner save')
    version, size, uid, profile, count = struct.unpack_from('<5I', data, 4)
    require((version, uid, profile, count) == (1, B, 3, 1) and len(data) == 24 + size + 320,
            'RFSW1 layout, active identity or owner count changed')
    bank = list(struct.unpack('<80I', data[24 + size:]))
    data = data[24:24 + size]
    require(data[:4] == b'RFNS', 'Missing living authored seat wrapper')
    version, size, count = struct.unpack_from('<3I', data, 4)
    require((version, count) == (1, 1) and len(data) == 16 + size + 24, 'RFNS1 layout/count changed')
    seat = list(struct.unpack_from('<6I', data, 16 + size))
    require(seat == [ACTOR, A, recipe['seat']['index'], 1, 1, 0], 'RFNS lost living passive driver or assigned player control')
    data = data[16:16 + size]
    require(data[:4] == b'RFVA', 'Missing passive host pose/vitals')
    version, size, count = struct.unpack_from('<3I', data, 4)
    require((version, size, count) == (2, 160, 1) and len(data) == 16 + size + 80, 'RFVA2 layout/count changed')
    vehicle, passive = data[16:176], list(struct.unpack('<20I', data[176:]))
    require(vehicle[:4] == b'RFVC' and struct.unpack_from('<2I', vehicle, 4) == (3, 160) and
            struct.unpack_from('<2I', vehicle, 16) == (3, 3) and struct.unpack_from('<I', vehicle, 108)[0] == 1,
            'Saved B is not an occupied living gunner Jeep')
    vehicle = list(struct.unpack('<40I', vehicle))
    require(bank[:2] == [A, 1] and bank[3] == 0 and bank[15] == 0 and bank[79] == 0 and
            passive[:2] == [A, 0] and passive[19] == 0 and bank[16:28] == passive[2:14],
            'Passive A bank and actual host disagree on pose, identity or life')
    npc = sections[2]
    require(npc[:4] == b'RFNC' and struct.unpack_from('<I', npc, 4)[0] == 10 and
            struct.unpack_from('<I', npc, 16)[0] == 1 and len(npc) >= 664, 'Expected RFNC10 single original driver')
    actor = npc[64:]
    require(struct.unpack_from('<I', actor)[0] == ACTOR and struct.unpack_from('<I', actor, 8)[0] == 0 and
            struct.unpack_from('<i', actor, 524)[0] == 13 and struct.unpack_from('<f', actor, 20)[0] == recipe['driver']['authored_health'] and
            struct.unpack_from('<I', actor, 552)[0] == 0, 'Saved actor is not the original living seated miner')
    events = sections[4]
    require(len(events) == 384, 'Expected exactly two saved authored events')
    timer, slay = events[:192], events[192:]
    require(timer[:4] == slay[:4] == b'RFEC' and struct.unpack_from('<2I', timer, 4) == (3, 192) and
            struct.unpack_from('<2I', timer, 16) == (TIMER, 48) and struct.unpack_from('<I', slay, 16)[0] == SLAY,
            'Ordinary event checkpoint lost saved Delay/Slay owners')
    remaining = struct.unpack_from('<i', timer, 160)[0]
    require(RETURN_FRAMES * 1000 / 60 < remaining < WRECK_FRAMES * 1000 / 60 and
            struct.unpack_from('<i', slay, 160)[0] == -1, 'Saved Delay must expire only in the alternate continuation')
    return dict(bytes=len(payload), sha256=sha(payload), seat=seat, bank=bank, passive=passive,
        active_ammo=vehicle[26], active_rng=vehicle[30], active_pose=vehicle[8:20],
        active_health=vehicle[6], active_armor=vehicle[7], parked_ammo=bank[4], parked_rng=bank[6],
        parked_pose=passive[2:14], npc_position=list(struct.unpack_from('<3I', actor, 28)),
        timer_remaining_ms=remaining, formats=['RFWC', 'RFSW1', 'RFNS1', 'RFVA2', 'RFVC3', 'RFNC10', 'RFEC3'])


def validate_source(guest, payload, recipe):
    check_guest(guest, 1, len(payload))
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES}
    for row in rows.values():
        check_identity(row, recipe)
        require(row[27] == row[51] == 1 and row[31] == row[55] == 0 and row[124:130] == [0] * 6,
                'Source unexpectedly killed or ejected an owner')
        require([f(v) for v in row[28:30]] == [recipe['details']['authored_health'], recipe['details']['authored_armor']] and
                row[28:30] == row[52:54], 'Source changed either original chassis vitals')
    require(len({tuple(r[6:10]) for r in rows.values()}) == 1, 'Source changed a full registered handle')
    check_player(rows[190], A)
    check_player(rows[230])
    check_player(rows[348], B)
    require(rows[230][135:138] == [2, 0, 1] and rows[230][149] == 1, 'Ordinary A exit failed actual-body endpoint clearance')
    saved = saved_rows(payload, recipe)
    first = guest['extra']['rf_scene_vehicle_switch_history'][:32]
    require(first[:4] == [A, B, rows[190][8], rows[348][9]] and first[29] == 1 and
            guest['extra']['rf_scene_vehicle_switch'][1] == 1 and not any(guest['extra']['rf_scene_vehicle_switch_history'][32:]),
            'Source did not make exactly one ordinary A-to-B identity-preserving transfer')
    require(rows[190][110] == rows[348][110] == saved['parked_ammo'] and rows[348][113] == saved['active_ammo'] and
            first[9] > saved['parked_ammo'] and first[9] > saved['active_ammo'] and saved['parked_ammo'] != saved['active_ammo'],
            'Both owners must spend and retain independent real ammunition')
    require(rows[349][32:44] == saved['parked_pose'] and rows[349][56:68] == saved['active_pose'] and
            rows[349][68:71] == saved['npc_position'] and rows[310][32:44] == rows[348][32:44] == rows[349][32:44] and
            rows[310][68:80] == rows[348][68:80] == rows[349][68:80],
            'Saved owner pose differs from exact final349 observation or stable passive state')
    require(saved['bank'][8] == rows[348][139] and first[12:15] == saved['parked_pose'][:3] and
            max(abs(f(a) - f(b)) for a, b in zip(rows[190][32:35], saved['parked_pose'][:3])) <= .05,
            'Parking changed the retained role or moved stationary A beyond ordinary settling')
    require(guest['extra']['rf_scene_vehicle_state'][1:4] == [2, 1, 1] and
            guest['extra']['rf_scene_npc_seat_checkpoint'][0] == 1, 'Ordinary boards/exit/seat capture failed')
    return dict(result='PASS', saved=saved, observations={str(k): describe(v) for k, v in rows.items()})


def validate_loaded(guest, saved, recipe, phase):
    check_guest(guest, phase, saved['bytes'])
    frames = RETURN_FRAMES if phase == 2 else WRECK_FRAMES
    rows = {frame: observed(guest, frame) for frame in SAMPLE_FRAMES if frame < frames}
    require(len({tuple(r[6:10]) for r in rows.values()}) == 1, 'Fresh continuation changed a full registered handle')
    initial = rows[0]
    require(initial[32:44] == saved['parked_pose'] and initial[56:68] == saved['active_pose'] and
            initial[68:71] == saved['npc_position'] and initial[110:112] == [saved['parked_ammo'], saved['parked_rng']] and
            initial[113:115] == [saved['active_ammo'], saved['active_rng']], 'Fresh load changed exact saved poses or independent ammo/RNG')
    check_player(rows[40], B)
    require(guest['extra']['rf_scene_npc_seat_checkpoint'][:4] == [0, 1, 1, 1] and
            not guest['extra']['rf_scene_npc_seat_checkpoint'][5], 'Living passive RFNS relation did not restore once')
    for row in rows.values():
        released = phase == 3 and row[127] == 1
        check_identity(row, recipe, not released)
        require(row[110:112] == [saved['parked_ammo'], saved['parked_rng']] and
                row[113:115] == [saved['active_ammo'], saved['active_rng']], 'Continuation changed either ammunition bank/RNG')
        require(row[52:54] == [saved['active_health'], saved['active_armor']] and row[51] == 1 and row[55] == 0,
                'Passive A change damaged independently owned B')
        if not released:
            require(row[28:30] == saved['passive'][14:16] and row[27] == 1 and row[31] == 0,
                    'Living A changed its saved authored health or armor')
        if phase == 3 or row[4] == B:
            require(row[32:44] == saved['parked_pose'], 'Parked original host moved during continuation')
    x = guest['extra']
    if phase == 2:
        check_player(rows[80])
        check_player(rows[118], A)
        check_player(rows[138], A)
        require(rows[80][135:138] == [2, 0, 1] and rows[80][149] == 1, 'Ordinary B exit failed full-body endpoint clearance')
        require(not any(x['rf_scene_script_slays']) and not any(x['rf_scene_vehicle_wreck_exit']), 'Primary endpoint ran past saved wreck deadline')
        swap = x['rf_scene_vehicle_switch_apply']
        require(swap[:4] == [B, A, initial[9], initial[8]] and swap[8:10] == [saved['active_ammo'], saved['parked_ammo']] and
                swap[29] == 1 and 99 <= swap[30] <= 101 and x['rf_scene_vehicle_switch'][1] == 1 and
                x['rf_scene_vehicle_state'][1:4] == [1, 1, 1], 'Ordinary return to A did not retain ownership/ammunition')
    else:
        settling = []
        for row in rows.values():
            check_player(row, B)
            delta = [abs(f(value) - f(start)) for value, start in zip(row[56:68], initial[56:68])]
            measured = dict(horizontal_m=math.hypot(delta[0], delta[2]), vertical_m=delta[1], basis_component=max(delta[3:]))
            require(all(measured[key] <= limit for key, limit in B_SETTLING_LIMITS.items()),
                    f'Independently simulated occupied B exceeded measured settling bounds: {measured}')
            settling.append(measured)
        before, end = rows[230], rows[318]
        require(before[124:130] == [0] * 6 and before[119] not in (0, NONE) and
                end[119] == NONE and end[124:126] == [1, A], 'Saved Delay did not dispatch the expected host once')
        slay, wreck, apply = x['rf_scene_script_slays'], x['rf_scene_vehicle_wreck_exit'], x['rf_scene_vehicle_wreck_exit_apply']
        require(slay[:3] == [1, 1, A] and f(slay[3]) <= 0 and slay[5] == 0 and
                wreck[1] == 1 and wreck[3] == 0 and wreck[5:7] == [initial[7], initial[8]] and wreck[10:] == [ACTOR, A],
                'Ordinary host Slay/wreck callback did not release its exact living driver')
        expected_frame = saved['timer_remaining_ms'] * 60 / 1000
        require(abs(apply[0] - expected_frame) <= 4 and apply[1:7] == [initial[7], initial[8], NONE, NONE, NONE, 2] and
                apply[7] == initial[18] and apply[27] == 0 and apply[23:26] == saved['parked_pose'][:3],
                'Wreck publication used another host, changed driver health or retained endpoints')
        require(all(f(value) == 0. for value in apply[14:23]),
                'Stationary passive wreck exit inherited nonzero host or selected-vehicle momentum')
        require(end[27] == 0 and end[31] == 1 and f(end[28]) <= 0 and end[135:138] == [1, 0, 1] and
                end[149] == 3 and end[147] > 0 and end[148] > 0 and min(f(end[144]), f(end[145])) >= -.0021,
                'Released living driver lacks actual-body world and both-hull clearance')
        require(x['rf_scene_vehicle_switch'][1] == 0 and x['rf_scene_vehicle_state'][1:4] == [0, 0, 1] and
                not any(x['rf_scene_live_death_audio']), 'Alternate continuation switched/reboarded or killed the driver')
    result = dict(result='PASS', source_payload_sha256=saved['sha256'], observations={str(k): describe(v) for k, v in rows.items()},
                  malformed_admission=x['rf_scene_passive_jeep_restore_audit'], free_pages=guest['free_pages'])
    if phase == 3:
        result['B_rigid_settling'] = dict(limits=B_SETTLING_LIMITS,
            observed_maximum={key: max(row[key] for row in settling) for key in B_SETTLING_LIMITS})
    return result


def phase_inputs(level, phase):
    result = {'scene-fixture.vpp': level.read_bytes(),
        'campaign-level.bin': b'scene-fixture.vpp'.ljust(64, b'\0') + b'ctf06.rfl'.ljust(64, b'\0'),
        'campaign-spawn.flag': b'', 'player-control.flag': b'', 'scene-preview.flag': b'',
        CONFIG: U(0x44504a50, phase, 0, 0), 'player-replay.bin': replay(phase),
        'world-hdd-save.flag' if phase == 1 else 'world-hdd-load.flag': b'1'}
    if phase == 1:
        result['campaign-setup.bin'] = U(TIMER)
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
        'passive-jeep-driver-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
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
        limitations=LIMITATIONS, hdd=str(hdd), fresh_loads_use_snapshot=True)
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
        for phase, label, frames in ((1, 'source', SOURCE_FRAMES), (2, 'return', RETURN_FRAMES), (3, 'wreck', WRECK_FRAMES)):
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
            guest = run_guest(folder, label, hdd, frames, args.seconds, snapshot=phase != 1,
                capture_world=phase == 1, extra_symbols=SYMBOLS, allow_guest_error=True)
            report['phases'][label] = guest
            write_report()
            if phase == 1:
                payload = (folder / label / 'xbox-world.rfwc').read_bytes()
                report['validation'][label] = validate_source(guest, payload, recipe)
                saved = report['validation'][label]['saved']
            else:
                report['validation'][label] = validate_loaded(guest, saved, recipe, phase)
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
