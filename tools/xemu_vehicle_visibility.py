"""Bounded Xbox passive Jeep Hide/UnHide and ordinary save/fresh-load.

Reuse the established enemy-free CTF06 two-Jeep fixture. B is authored hidden
and hostile in the disposable fixture; all other original non-transform bytes
are retained. Normal Delay/UnHide/Invert events reveal, hide and reveal B;
ordinary Use/fire promotes it, then Hide parks A invisibly before saving.
Fresh load must retain B and reject promotion of hidden A. Read-only guest
telemetry only: no state writes, PC gameplay, images, host input or routes.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import re
import struct

from build_fragment_platform_fixture import read_entry, U
from check_ai_projectile_ordinary import archive
from check_hit_event import event
from inspect_levels import inspect
from xemu_turret_combat import entity_rows, f
from xemu_npc_turret_seat import record_details
from xemu_vehicle_switch import A, B, SYMBOLS as SWITCH_SYMBOLS, prepare_level as prepare_pair
from xemu_vehicle_switch_save import saved_rows as saved_switch_rows
from xemu_npc_jeep_detached_save import component
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

START, SHOW_B, SHOW_TIMER, HIDE_B, HIDE_TIMER, AGAIN_TIMER, SHOW_A, HIDE_A = range(915600, 915608)
SAVE_FRAMES, LOAD_FRAMES, HIDDEN = 420, 140, 0x4000
SYMBOLS = dict(SWITCH_SYMBOLS, rf_scene_vehicle_visibility=8,
    rf_scene_vehicle_visibility_history=80, rf_scene_passive_draw=6,
    rf_scene_passive_collision=8, rf_scene_passive_damage=8,
    rf_scene_world_load_reject=3, rf_scene_checkpoint_world_reject=9,
    rf_scene_vehicle_restore_trace=8, rf_scene_npc_checkpoint_reject_state=6,
    rf_scene_vehicle_switch_restore=16)
LIMITATIONS = ('Parked same-class Jeep visibility and one ordinary save/fresh boot only. '
    'Hidden admission is measured through the real homing predicate and render admission; '
    'no new guided shot, visual/audio appearance, moving/grouped/occupied Hide, or dead-owner resurrection claim. '
    'Load Use may reboard the still-visible B; it must never promote hidden A.')


def command(uid, kind, name, links, delay=0):
    value = bytearray(event(uid, kind, name, links))
    struct.pack_into('<f', value, 4+2+len(kind)+12+2+len(name)+1, delay)
    return bytes(value)



def inspect_events(data, types):
    """Inspect only this fixture's ordinary v180 event forms, without RF.exe/pefile."""
    at = 0
    def take(size):
        nonlocal at
        if size < 0 or at+size > len(data): raise RuntimeError('Staged event exceeds section')
        value = data[at:at+size]; at += size; return value
    def word(): return struct.unpack('<I',take(4))[0]
    def string(): return take(struct.unpack('<H',take(2))[0]).decode('ascii')
    rows = []
    for _ in range(word()):
        start = at; uid, kind = word(), string()
        position = struct.unpack('<3f',take(12)); name = string()
        header = take(1)[0]; delay = struct.unpack('<f',take(4))[0]
        flags = list(take(2)); words_ = [word(),word()]
        values = struct.unpack('<2f',take(8)); texts = [string(),string()]
        links = [word() for _ in range(word())]; color = take(4)
        if kind not in ('Delay','UnHide','Invert') or types.index(kind) not in (48,50,3):
            raise RuntimeError('Unexpected fixture event type')
        if position != (0,0,0) or header or flags != [0,0] or words_ != [0,0] or values != (0,0) or texts != ['', ''] or color != b'\xff'*4:
            raise RuntimeError('Staged event modified an undeclared field')
        rows.append(dict(uid=uid,type=kind,type_index=types.index(kind),name=name,
                         delay=delay,links=links,offset=start,bytes=at-start))
    if at != len(data): raise RuntimeError('Staged event section has trailing bytes')
    return rows


def creation_offsets(row):
    """v180 field offsets, matching rf_level_entity_spawn_read."""
    raw = row['raw']; at = row['transform']+48
    def string():
        nonlocal at
        size = struct.unpack_from('<H', raw, at)[0]; at += 2+size
        if at > len(raw): raise RuntimeError('Entity string exceeds its record')
    string(); friendliness = at+5; at += 13; string(); string(); at += 29
    for _ in range(7): string()
    at += 18
    return friendliness, at+1


def replay(load=False):
    frames = LOAD_FRAMES if load else SAVE_FRAMES
    if load:
        uses, cycles = (60, 100), ()
    else:
        uses, cycles = (90, 210, 310), (110, 330)
    return b'RFI6'+U(48)+b''.join(struct.pack('<5f7I', 0,0,0,0,0,0,0,
        int(frame in uses), int(not load and (130 <= frame < 150 or 350 <= frame < 358)),
        0, int(frame in cycles), 0) for frame in range(frames))


def prepare_level(folder):
    path, recipe = prepare_pair(folder)
    raw = read_entry(path, 'ctf06.rfl')
    meta = inspect(io.BytesIO(raw), dict(offset=0, size=len(raw), name='ctf06.rfl'))
    rows = entity_rows(raw)
    if [r['uid'] for r in rows] != [A, B]: raise RuntimeError('Unexpected established Jeep pair')
    b = rows[1]; friendliness, hidden = creation_offsets(b)
    if struct.unpack_from('<I', b['raw'], friendliness)[0] != 1 or b['raw'][hidden]:
        raise RuntimeError('Expected original friendly, visible Jeep record')
    patched = bytearray(b['raw'])
    struct.pack_into('<I', patched, friendliness, 0); patched[hidden] = 1
    allowed = {hidden, *range(friendliness, friendliness+4)}
    if any(old != new and i not in allowed for i, (old, new) in enumerate(zip(b['raw'], patched))):
        raise RuntimeError('Modified Jeep outside the declared fixture creation fields')
    records = [rows[0]['raw'], bytes(patched)]
    event_rows = [
        event(START, 'Delay', 'visibility_setup', (SHOW_TIMER,HIDE_TIMER,AGAIN_TIMER,HIDE_A)),
        event(SHOW_B, 'UnHide', 'show_hidden_jeep', (B,)),
        command(SHOW_TIMER, 'Delay', 'show_at_two_seconds', (SHOW_B,), 2),
        event(HIDE_B, 'Invert', 'hide_revealed_jeep', (SHOW_B,)),
        command(HIDE_TIMER, 'Delay', 'hide_at_four_seconds', (HIDE_B,), 4),
        command(AGAIN_TIMER, 'Delay', 'reveal_before_boarding', (SHOW_B,), 5),
        event(SHOW_A, 'UnHide', 'parked_first_jeep_visibility', (A,)),
        command(HIDE_A, 'Invert', 'hide_parked_first_jeep', (SHOW_A,), 6),
    ]
    events = U(len(event_rows))+b''.join(event_rows)
    # Use the checked-in port type table. RF.exe is deliberately not required.
    source = (ROOT/'src/core/event.c').read_text()
    types = dict((int(index), name) for name,index in re.findall(r'"([^"]+)"\s*,\s*/\*\s*(\d+)\s*\*/', source))
    if set(types) != set(range(90)): raise RuntimeError('Port event type table differs')
    decoded = inspect_events(events, [types[i] for i in range(90)])
    if [r['uid'] for r in decoded] != list(range(START, HIDE_A+1)):
        raise RuntimeError('Staged normal event graph failed exact parse')
    replacements = {0x30000: U(2)+b''.join(records), 0x600: events}
    out = bytearray(raw[:meta['sections'][0]['offset']]); offsets = {}
    for section in meta['sections']:
        kind = int(section['type'],16)
        payload = replacements.get(kind, raw[section['offset']+8:section['offset']+8+section['size']])
        offsets[kind] = len(out); out += U(kind,len(payload))+payload
    struct.pack_into('<II', out, 12, offsets[0x70000], offsets[0x1000000])
    inspect(io.BytesIO(out), dict(offset=0,size=len(out),name='ctf06.rfl'))
    check = entity_rows(out)
    if [r['raw'] for r in check] != records or any(record_details(r) != recipe['details'] for r in check):
        raise RuntimeError('Owner record/vitals round trip failed')
    archive(path, [('ctf06.rfl',out)])
    recipe.update(scope=__doc__, frames=SAVE_FRAMES, load_frames=LOAD_FRAMES,
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        staged_fields=['existing pair UID/transforms/spawn', 'B authored hidden byte: 0 to 1',
                       'B friendliness: 1 to 0 for actual homing-admission observation', 'normal event graph'],
        staged_record_sha256=[hashlib.sha256(r).hexdigest() for r in records],
        creation_offsets=dict(friendliness=friendliness,hidden=hidden),
        events=decoded, setup_event=START,
        replay=dict(probe_initial_hidden_B=60,board_A=90,gunner_A=110,reveal_B=120,
                    fire_A=[130,149],probe_revealed_B=180,exit_A=210,hide_B=240,
                    reveal_B_again=300,board_B=310,gunner_B=330,fire_B=[350,357],
                    hide_parked_A=360,ordinary_save=SAVE_FRAMES),
        load_replay=dict(probe_restored_B_and_hidden_A=40,exit_B=60,try_Use_near_hidden_A=100,
                         probe_after_Use=110), limitations=LIMITATIONS)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    (folder/'player-replay.bin').write_bytes(replay())
    (folder/'load-replay.bin').write_bytes(replay(True))
    return path, recipe


def live_probe(monitor, mapping):
    sample = {name: words(monitor,address(mapping,name),count) for name,count in SYMBOLS.items()}
    sample['frame'] = words(monitor,address(mapping,'rf_diagnostic'),58)[37]
    layout = words(monitor,address(mapping,'rf_scene_vehicle_homing_owner_layout'),4)
    visibility = words(monitor,address(mapping,'rf_scene_passive_visibility_layout'),2)
    stride, handle_at, uid_at, health_at = layout
    flags_at, destroyed_at = visibility
    count = words(monitor,address(mapping,'campaign_passive_vehicle_count'),1)[0]
    pointer = words(monitor,address(mapping,'campaign_passive_vehicles'),1)[0]
    if count != 1 or not pointer or not 0 < stride <= 16384 or any(x % 4 or x+4 > stride for x in layout[1:]+visibility):
        raise RuntimeError('Invalid single-passive-owner read-only telemetry ABI')
    sample['passive_layout'] = layout+visibility
    sample['owners'] = [dict(uid=words(monitor,pointer+i*stride+uid_at,1)[0],
        handle=words(monitor,pointer+i*stride+handle_at,1)[0],
        health_bits=words(monitor,pointer+i*stride+health_at,1)[0],
        flags=words(monitor,pointer+i*stride+flags_at,1)[0],
        destroyed=words(monitor,pointer+i*stride+destroyed_at,1)[0]) for i in range(count)]
    return sample


def full_handle(handle):
    if handle in (0,0xffffffff) or not handle >> 16: raise RuntimeError('Missing full generation-bearing handle')
    return handle


def check_run(result, frames, setup):
    if result['guest_phase'] != 5 or result['frames'] != frames or result['memory_bytes'] != 64*1024*1024 or result['free_pages'] <= 0:
        raise RuntimeError('Incomplete bounded stock-64-MiB Xbox run')
    for sample in [result['extra'],result['probe'],result['final_probe']]:
        if sample['rf_scene_setup_result'] != setup or sample['rf_scene_vehicle_state'][5] or sample['rf_scene_vehicle_switch'][8]:
            raise RuntimeError('Setup, owner or switch error')
        if any(sample['rf_scene_npc_seats']) or sample['rf_scene_vehicle_route_state'][0] or sample['rf_scene_vehicle_route_state'][3]:
            raise RuntimeError('Unexpected NPC occupancy or scripted route')
        if sample['rf_scene_enemy_combat'][2] or sample['rf_scene_enemy_combat'][7] or sample['rf_scene_passive_damage'][2] or sample['rf_scene_passive_damage'][6]:
            raise RuntimeError('Unexpected damage or NPC combat')
        if sample['rf_scene_passive_draw'][5] or sample['rf_scene_passive_collision'][7] or sample['rf_scene_vehicle_visibility'][7]:
            raise RuntimeError('Visibility, draw or hull error')


def owner(sample, uid, hidden, recipe):
    if len(sample['owners']) != 1 or sample['owners'][0]['uid'] != uid:
        raise RuntimeError('Wrong live passive UID')
    row = sample['owners'][0]; full_handle(row['handle'])
    if bool(row['flags'] & HIDDEN) != hidden or row['flags'] & 2 or row['destroyed'] or f(row['health_bits']) != recipe['details']['authored_health']:
        raise RuntimeError(f'Passive visibility/living health mismatch: {row}')
    return row


def saved_rows(payload, recipe):
    saved = saved_switch_rows(payload)  # Existing same-class RFSW1/RFVA2/RFVC3 contracts.
    outer = component(payload,11); inner_bytes = struct.unpack_from('<I',outer,8)[0]
    inner = outer[24:24+inner_bytes]; host_bytes = struct.unpack_from('<I',inner,8)[0]
    active = list(struct.unpack('<40I',inner[16:16+host_bytes]))
    passive = list(struct.unpack('<20I',inner[16+host_bytes:]))
    health, armor = recipe['details']['authored_health'], recipe['details']['authored_armor']
    if [f(active[6]),f(active[7]),f(passive[14]),f(passive[15])] != [health,armor,health,armor] or not passive[16]&HIDDEN or passive[19]:
        raise RuntimeError('Ordinary save changed vitals or omitted parked A hidden flag')
    saved.update(parked_flags=passive[16],parked_health_bits=passive[14],
                 active_health_bits=active[6],versions=dict(RFSW=1,RFVA=2,RFVC=3))
    return saved


def validate_source(result, payload, recipe):
    check_run(result,SAVE_FRAMES,[1,START,48,0])
    before, shown, end = result['probe'],result['final_probe'],result['extra']
    if not 60 <= before['frame'] < 90 or not 180 <= shown['frame'] < 210:
        raise RuntimeError('Missed required initial-hidden/revealed live windows')
    initial = owner(before,B,True,recipe); revealed = owner(shown,B,False,recipe)
    if initial['handle'] != revealed['handle'] or initial['flags'] ^ revealed['flags'] != HIDDEN:
        raise RuntimeError('Reveal replaced the owner or changed non-visibility flags')
    if before['rf_scene_vehicle_visibility'][0] or any(before['rf_scene_vehicle_visibility_history']):
        raise RuntimeError('B was changed before its delayed event')
    if before['rf_scene_passive_draw'][:5] != [1,0,0,0,0]:
        raise RuntimeError('Initially hidden passive B entered renderer submission')
    if shown['rf_scene_passive_draw'][0:2] != [1,1] or shown['rf_scene_passive_draw'][4] != B:
        raise RuntimeError('Revealed passive B did not enter the ordinary render path')
    if before['rf_scene_passive_collision'][0] or not before['rf_scene_passive_collision'][2]:
        raise RuntimeError('Hidden B entered hull queries, or no ground-query opportunity was observed')
    if not shown['rf_scene_passive_collision'][0] or not shown['rf_scene_passive_damage'][0]:
        raise RuntimeError('Revealed B did not enter actor/projectile hull query paths')
    if before['rf_scene_vehicle_selection'] != [2,2,1,0,0,0,0,1,A,3,2,0]:
        raise RuntimeError('Boot selection did not exclude authored-hidden B')
    if before['rf_scene_vehicle_state'][1:4] != [0,0,0] or shown['rf_scene_vehicle_state'][1:4] != [1,0,1] or shown['rf_scene_jeep_seats'][4] != 1 or shown['rf_scene_apc_primary'][1] < 1:
        raise RuntimeError('Initial normal boarding/gunner/real-fire sequence failed')
    a_handle = full_handle(before['rf_scene_vehicle_state'][12]); b_handle = initial['handle']
    if a_handle == b_handle: raise RuntimeError('Distinct Jeep owners alias a full handle')
    history = end['rf_scene_vehicle_visibility_history']
    rows = [history[i:i+10] for i in range(0,40,10)]
    expected = [(120,B,b_handle,1,0,1),(240,B,b_handle,0,1,0),
                (300,B,b_handle,1,0,1),(360,A,a_handle,0,None,0)]
    for row,(frame,uid,handle,visible,old_admit,new_admit) in zip(rows,expected):
        if abs(row[0]-frame)>2 or row[1:4] != [uid,handle,visible] or row[7] != 1 or row[9] != new_admit or row[8] not in (0,1) or (old_admit is not None and row[8] != old_admit):
            raise RuntimeError(f'Normal Hide/UnHide event identity/admission failed: {row}')
        if row[4]^row[5] != HIDDEN or bool(row[5]&HIDDEN) == bool(visible) or f(row[6]) != recipe['details']['authored_health']:
            raise RuntimeError(f'Hide/UnHide changed health or unrelated flags: {row}')
    if any(history[40:]) or [r[4:6] for r in rows[:3]] != [[initial['flags'],revealed['flags']], [revealed['flags'],initial['flags']], [initial['flags'],revealed['flags']]]:
        raise RuntimeError('Visibility callbacks repeated or lost exact flags')
    if end['rf_scene_vehicle_visibility'] != [4,2,2,0,A,a_handle,rows[-1][5],0]:
        raise RuntimeError('Unexpected visibility callback totals')
    if shown['rf_scene_vehicle_visibility_history'][:10] != rows[0] or any(shown['rf_scene_vehicle_visibility_history'][10:]):
        raise RuntimeError('Revealed snapshot did not follow exactly one normal event')
    sw, applied = end['rf_scene_vehicle_switch'],end['rf_scene_vehicle_switch_apply']
    if sw[1] != 1 or sw[4:8] != [A,B,a_handle,b_handle] or sw[9] or sw[15] != B:
        raise RuntimeError('Originally hidden, revealed B could not be promoted by ordinary Use')
    if applied[:4] != [A,B,a_handle,b_handle] or applied[29] != 1 or not 309 <= applied[30] <= 311 or applied[31]:
        raise RuntimeError('Use handoff lost exact owner identity/registry coherence')
    if end['rf_scene_vehicle_switch_history'][:32] != applied or any(end['rf_scene_vehicle_switch_history'][32:]):
        raise RuntimeError('Unexpected additional vehicle exchange')
    if [f(v) for v in applied[4:8]] != [recipe['details']['authored_health']]*2+[recipe['details']['authored_armor']]*2:
        raise RuntimeError('Normal handoff changed authored chassis vitals')
    a_ammo, b_ammo = shown['rf_scene_apc_primary'][7],end['rf_scene_apc_primary'][7]
    if applied[8] != a_ammo or applied[9] <= a_ammo or applied[9] <= b_ammo or a_ammo == b_ammo:
        raise RuntimeError('The two real guns did not retain independently spent ammunition')
    if end['rf_scene_vehicle_state'][1:4] != [2,1,1] or end['rf_scene_vehicle_state'][12] != b_handle or end['rf_scene_jeep_seats'][4] != 1:
        raise RuntimeError('Save did not finish occupied in the revealed second Jeep')
    if end['rf_scene_passive_draw'][:5] != [1,0,0,0,0]:
        raise RuntimeError('Finally hidden parked A still entered the render path')
    state = result['checkpoint_state']
    if state[9] != 1 or state[3] or state[4] != len(payload): raise RuntimeError('Ordinary source world save failed')
    saved = saved_rows(payload,recipe)
    if saved['active_ammo'] != b_ammo or saved['parked_ammo'] != a_ammo or saved['parked_flags'] != rows[-1][5]:
        raise RuntimeError('Save disagrees with actual live ammo/visibility')
    if saved['active_position'] != end['rf_scene_vehicle_state'][6:9] or saved['parked_position'] != applied[12:15]:
        raise RuntimeError('Save changed either actual parked/active pose')
    return dict(saved=saved,visibility_history=rows,source_handles=dict(A=a_handle,B=b_handle),
                initially_hidden=initial,revealed=revealed,exchange=applied,
                render_admission=dict(hidden=before['rf_scene_passive_draw'],shown=shown['rf_scene_passive_draw'],hidden_A=end['rf_scene_passive_draw']))


def validate(source, loaded, payload, recipe):
    report = validate_source(source,payload,recipe); saved = report['saved']
    check_run(loaded,LOAD_FRAMES,[0,0,0,0])
    pre, after, end = loaded['probe'],loaded['final_probe'],loaded['extra']
    if not 40 <= pre['frame'] < 60 or not 110 <= after['frame'] < LOAD_FRAMES:
        raise RuntimeError('Missed restored/after-Use live windows')
    parked = owner(pre,A,True,recipe); parked_after = owner(after,A,True,recipe)
    if parked != parked_after or parked['flags'] != saved['parked_flags']:
        raise RuntimeError('Fresh load or Use mutated the hidden parked owner')
    state = loaded['checkpoint_state']
    if state[8] != 1 or state[0] or state[1] != len(payload) or any(end['rf_scene_world_load_reject']):
        raise RuntimeError('Fresh ordinary world load failed')
    active_handle = full_handle(pre['rf_scene_vehicle_state'][12])
    if active_handle == parked['handle']: raise RuntimeError('Restored active/passive identities alias')
    expected = [1,B,3,1,1,A,saved['parked_ammo'],saved['parked_rng'],0,0,0,
                saved['active_ammo'],saved['active_rng'],active_handle,parked['handle'],0]
    for sample in (pre,after,end):
        if sample['rf_scene_vehicle_switch_restore'] != expected or sample['rf_scene_vehicle_selection'][8:11] != [B,3,3]:
            raise RuntimeError('Fresh boot rejected originally-hidden revealed B or changed parked bank')
        if sample['rf_scene_vehicle_visibility'][0] or any(sample['rf_scene_vehicle_visibility_history']) or any(sample['rf_scene_setup_result']):
            raise RuntimeError('Load replayed source setup/visibility events')
        if sample['rf_scene_vehicle_switch'][1] or any(sample['rf_scene_vehicle_switch_history']) or sample['rf_scene_vehicle_state'][12] != active_handle:
            raise RuntimeError('Ordinary Use incorrectly promoted hidden parked A')
        if sample['rf_scene_apc_primary'][1] or sample['rf_scene_apc_primary'][7] != saved['active_ammo'] or f(sample['rf_scene_vehicle_damage'][0]) != recipe['details']['authored_health']:
            raise RuntimeError('Fresh continuation changed B ammunition/health or replayed firing')
        if sample['rf_scene_passive_draw'][:5] != [1,0,0,0,0]:
            raise RuntimeError('Hidden saved A was rendered after fresh boot')
        # Counters include pre-restoration startup queries. Once restored,
        # the hidden owner must add no actor/projectile hull queries or hits.
        if sample['rf_scene_passive_collision'][:2] != pre['rf_scene_passive_collision'][:2] or \
           sample['rf_scene_passive_damage'][:2] != pre['rf_scene_passive_damage'][:2]:
            raise RuntimeError('Hidden saved A entered actor/projectile hull queries after the restored probe')
    if after['rf_scene_passive_collision'][2] <= pre['rf_scene_passive_collision'][2]:
        raise RuntimeError('No live collision-query opportunities followed restoration')
    if pre['rf_scene_vehicle_state'][1:4] != [0,0,1] or pre['rf_scene_jeep_seats'][4] != 1:
        raise RuntimeError('Saved B did not restore occupied without an extra boarding')
    sw = end['rf_scene_vehicle_switch']; live = end['rf_scene_vehicle_state']
    if live[2] != 1 or live[1] not in (0,1) or live[3] != live[1] or not sw[0] or not 99 <= sw[14] <= 101:
        raise RuntimeError('Load did not execute ordinary exit and a real post-exit Use attempt')
    report.update(result='PASS',restore=expected,hidden_owner_after_load=parked,
                  hidden_owner_after_Use=parked_after,load_Use_reboarded_visible_B=bool(live[3]),
                  free_pages=[source['free_pages'],loaded['free_pages']],limitations=LIMITATIONS)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',type=Path,help='Prepare and inspect private fixture bytes; no build or emulator')
    parser.add_argument('--resume-saved-run',type=Path,help='Reuse source save and matching current test HDD after a failed load')
    args = parser.parse_args()
    if args.prepare_only:
        print(prepare_level(args.prepare_only)[0]); return
    require_no_project_xemu(ROOT)
    hdd = prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('vehicle-visibility-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture, recipe = prepare_level(folder/'level')
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','scene-preview.flag'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL',recipe=recipe,phases={})
    try:
        for name in names: (DISC/name).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        for name in ('campaign-spawn.flag','player-control.flag','scene-preview.flag'): (DISC/name).write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(U(START))
        (DISC/'world-hdd-save.flag').write_bytes(b'1'); (DISC/'player-replay.bin').write_bytes(replay())
        if args.resume_saved_run:
            prior = args.resume_saved_run.resolve()
            prior_recipe = json.loads((prior/'level/recipe.json').read_text())
            if prior_recipe['fixture_sha256'] != recipe['fixture_sha256']: raise RuntimeError('Resume fixture provenance differs')
            source = json.loads((prior/'save/result.json').read_text())
            payload = (prior/'save/xbox-world.rfwc').read_bytes(); report['resumed_source'] = str(prior)
        else:
            build(folder,'save')
            source = run_guest(folder,'save',hdd,SAVE_FRAMES,600,capture_world=True,
                extra_symbols=SYMBOLS,probe=live_probe,probe_frame=60,
                final_probe=live_probe,final_probe_frame=180,allow_guest_error=True)
            payload = (folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save'] = source; report['source_validation'] = validate_source(source,payload,recipe)
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        (DISC/'campaign-setup.bin').unlink(); (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1'); (DISC/'player-replay.bin').write_bytes(replay(True))
        build(folder,'load')
        loaded = run_guest(folder,'load',hdd,LOAD_FRAMES,420,extra_symbols=SYMBOLS,
            probe=live_probe,probe_frame=40,final_probe=live_probe,final_probe_frame=110,allow_guest_error=True)
        report['phases']['load'] = loaded; report.update(validate(source,loaded,payload,recipe))
    except Exception as exc:
        report['error'] = str(exc); raise
    finally:
        for name,data in original.items():
            if data is None: (DISC/name).unlink(missing_ok=True)
            else: (DISC/name).write_bytes(data)
        try: build(folder,'restore')
        except Exception as exc:
            report['result'] = 'FAIL'; report['restore_error'] = str(exc); raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']: report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']: raise RuntimeError('Disc restoration failed')


if __name__ == '__main__': main()
