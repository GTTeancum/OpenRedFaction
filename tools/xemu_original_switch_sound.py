"""Parent-only 17:00 original L7S1 Switch3714 ambient-volume check.

Source preparation only. Run once after the parent's Xbox build. This wrapper
never builds, creates a scene fixture, saves/loads a world, copies the HDD,
captures images/audio, supplies host input, or retries/adjusts the approach.
It packages the existing XBE and uses the owned base HDD with XEMU -snapshot.
The actual OFF write, three UID-qualified slots, Switch state and native audio
telemetry are separate observations. No audibility, ON, save or FPS claim.
"""
import argparse
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
os.environ.setdefault('RF_XEMU_ROOT', '/workspace/shared/xemu-inputs')
os.environ.setdefault('RF_XEMU_BINARY', '/workspace/shared/xemu/squashfs-root/usr/bin/xemu')

RFL_SHA256 = '8fd6c6d69bdd4af2a365672b8ca9620f4139f022916915651bce19492d743e53'
FRAMES, MOVE_BEGIN, MOVE_END, EARLY_FRAME, FINAL_FRAME = 900, 60, 180, 30, 850
MOVE = (-0.4320959, 0.0, 0.2515813)
SOUNDS, SWITCHES = (4062, 4063, 4064), (3685, 3916, 3714)
VOLUME_BITS = 0x3e99999a
UINT_MAX = 0xffffffff
# Alarm_Siren45 appends active/voice after retired; existing field offsets stay.
# Source contract: include/rf/event.h, final Xbox32 layout116 bytes.
EVENT_BYTES, EVENT_WORDS, AUTHORED_EVENT_BYTES = 116, 29, 1148
SYMBOLS = {
    'rf_scene_switch_ambient': 8, 'rf_scene_cutscene': 12,
    'rf_scene_switch_runtime': 8, 'rf_scene_switch_detail': 8,
    'rf_scene_trigger_contacts': 6, 'rf_scene_event_ticks': 12,
    'rf_scene_ambient_audio': 8, 'rf_scene_ambient_schedule': 6,
    'rf_scene_ambient_records': 3, 'rf_scene_ambient_instances': 4,
    'rf_scene_live_audio': 8, 'rf_xbox_audio_diagnostic': 12,
    'rf_xbox_music_diagnostic': 12,
    'rf_scene_player_spawn_diagnostic': 19, 'rf_scene_setup_result': 4,
    'rf_scene_player_life': 8, 'rf_scene_section_autosave': 4,
    'rf_scene_script_grants': 8, 'rf_scene_combat': 8,
    'rf_scene_actor_turn_enabled': 1, 'rf_scene_actor_look_enabled': 1,
}
OWNER_SYMBOLS = ('campaign_ambient_instances', 'campaign_ambient_slots',
                 'campaign_events', 'campaign_registry', 'scene_actor_body')


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def floats(value):
    return list(struct.unpack('<' + 'f' * len(value), struct.pack('<' + 'I' * len(value), *value)))


def exact_address(mapping, name):
    """Require one complete linker-map symbol token, never a name suffix.

    The 17:00 observer's inherited unanchored lookup for campaign_events
    selected _rf_scene_campaign_events at0041e8ac instead of the actual
    _campaign_events owner at003e5528. Preserve that failed run unchanged.
    """
    pattern = (r'(?m)^\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+_' + re.escape(name) +
               r'\s+([0-9a-fA-F]+)(?=\s|$)')
    matches = re.findall(pattern, mapping)
    demand(len(matches) == 1, 'Missing/ambiguous exact Xbox symbol: ' + name)
    return int(matches[0], 16)


def replay():
    neutral = struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    moving = struct.pack('<5f7I', *MOVE, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        moving if MOVE_BEGIN <= frame < MOVE_END else neutral for frame in range(FRAMES))


def original_recipe(root, read_entry, inspect_level, inspect_triggers):
    data = read_entry(root / 'Installed_Game/levels2.vpp', 'L7S1.rfl')
    demand(hashlib.sha256(data).hexdigest() == RFL_SHA256, 'Original L7S1 hash differs')
    meta = inspect_level(io.BytesIO(data), dict(offset=0, size=len(data), name='L7S1.rfl'))
    demand(meta['version'] == 180, 'Require original version180')

    def section(kind):
        rows = [row for row in meta['sections'] if row['type'] == hex(kind)]
        demand(len(rows) == 1, 'Missing/ambiguous original section')
        row = rows[0]
        return data[row['offset'] + 8:row['offset'] + 8 + row['size']]

    start = list(struct.unpack('<12I', section(0x70000)))
    spawn_words = [1] + start[:3] + start[6:12] + start[3:6]
    matches = [row for row in inspect_triggers(section(0x60000)) if row['uid'] == 5504]
    demand(len(matches) == 1, 'Original trigger5504 absent/ambiguous')
    trigger = matches[0]
    demand(trigger['links'] == [5505] and trigger['flags'] == [0] * 5 and
           trigger['tail_flag'] == 0 and trigger['unknown_word'] == 1 and
           trigger['shape'] == 1 and trigger['dimensions_disk'] == [4, 2, 4] and
           trigger['value_byte'] == 0 and trigger['script'] == '', 'Original contact trigger differs')
    ambient = section(0x500)
    count = struct.unpack_from('<I', ambient)[0]
    demand(3 <= count <= 1024, 'Invalid authored ambient count')
    cursor, rows = 4, {}
    for unused in range(count):
        demand(cursor + 19 <= len(ambient), 'Truncated ambient header')
        uid = struct.unpack_from('<I', ambient, cursor)[0]
        position_bits = list(struct.unpack_from('<3I', ambient, cursor + 4))
        length = struct.unpack_from('<H', ambient, cursor + 17)[0]
        cursor += 19
        demand(cursor + length + 16 <= len(ambient), 'Truncated ambient record')
        name = ambient[cursor:cursor + length].decode('cp1252')
        cursor += length
        near, volume, rolloff, flags = struct.unpack_from('<4I', ambient, cursor)
        cursor += 16
        if uid in SOUNDS:
            demand(uid not in rows, 'Duplicate original sound UID')
            demand(name == 'Amb_Energy_02.wav' and volume == VOLUME_BITS and flags == 0,
                   'Original immediate energy sound differs')
            rows[uid] = dict(uid=uid, name=name, position_bits=position_bits,
                             near_bits=near, volume_bits=volume, rolloff_bits=rolloff)
    demand(cursor == len(ambient) and set(rows) == set(SOUNDS), 'Missing original sound or trailing bytes')
    return dict(level='L7S1.rfl', archive='levels2.vpp', rfl_sha256=RFL_SHA256,
        spawn_words=spawn_words, original_spawn=floats(start[:3]), trigger=trigger,
        sounds=rows, ambient_count=count, frames=FRAMES, move_begin=MOVE_BEGIN,
        move_end=MOVE_END, move_xyz=list(MOVE), early_probe=EARLY_FRAME, final_probe=FINAL_FRAME,
        chain='Trigger5504 -> Delay5505 -> Cutscene5495 point2 -> Delay3726(3.3s) -> Switch3714 -> ambient4064',
        input='RFI6; frames60..179 half-strength source-derived horizontal approach; all other input zero',
        limitation='One bounded original-spawn approach, not a proven collision-clear route. Stop on failure; no route adjustment.')


def observe(monitor, mapping, words, recipe, phase, persist):
    audit = dict(phase=phase, complete=False, reads=[])
    persist(audit)

    def read_words(address, count, symbol=None):
        entry = dict(address=address, count=count, symbol=symbol, status='REQUESTED')
        audit['reads'].append(entry)
        persist(audit)
        try:
            result = words(monitor, address, count)
        except BaseException as error:
            entry.update(status='READ_FAILED', error=type(error).__name__ + ': ' + str(error))
            persist(audit)
            raise
        entry.update(status='READ', words=result)
        # Raw owner headers survive every later census/identity predicate.
        persist(audit)
        return result

    def get(name, count):
        return read_words(exact_address(mapping, name), count, name)

    def memory(address, count):
        demand(isinstance(address, int) and address >= 0x10000 and address % 4 == 0 and
               0 < count <= 1024 * EVENT_WORDS and address + count * 4 <= 0x100000000,
               'Invalid/beyond-bound guest pointer read')
        result = []
        for offset in range(0, count, 256):
            result += read_words(address + offset * 4, min(256, count - offset))
        return result

    diagnostic = get('rf_diagnostic', 58)
    frame = diagnostic[37]
    audit['frame'] = frame
    persist(audit)
    demand(diagnostic[2] == 2, 'Owner probe did not occur in the live scene')
    demand(EARLY_FRAME <= frame < MOVE_BEGIN if phase == 'before' else FINAL_FRAME <= frame < FRAMES,
           'Missed required live probe window: ' + phase + ' frame' + str(frame))
    raw = {name: get(name, count) for name, count in SYMBOLS.items()}
    demand(raw['rf_scene_player_spawn_diagnostic'][:13] == recipe['spawn_words'], 'Original spawn/basis changed')
    owner = get('campaign_ambient_instances', 4)
    demand(3 <= owner[1] <= recipe['ambient_count'] and owner[1] + owner[2] == recipe['ambient_count'] and
           owner[3] == 16 + recipe['ambient_count'] * 44, 'Ambient owner census/ABI differs')
    instances = memory(owner[0], owner[1] * 11)
    slots = get('campaign_ambient_slots', 25 * 6)
    sounds = {}
    for index in range(owner[1]):
        row = instances[index * 11:(index + 1) * 11]
        uid = row[0]
        if uid not in SOUNDS:
            continue
        demand(uid not in sounds and row[1] < 2600 and row[2] < 25, 'Missing/duplicate sound identity or slot')
        authored = recipe['sounds'][uid]
        demand(row[3:6] == authored['position_bits'] and row[6:9] ==
               [authored['near_bits'], authored['volume_bits'], authored['rolloff_bits']] and row[9] == 0,
               'Ambient instance is not the original UID-qualified record')
        slot = slots[row[2] * 6:(row[2] + 1) * 6]
        demand(slot[0] == row[1] and slot[2:5] == row[3:6], 'Ambient slot ownership mismatch')
        demand(all(math.isfinite(value) for value in floats(slot[2:6])), 'Nonfinite ambient slot')
        sounds[uid] = dict(uid=uid, instance_pointer=owner[0] + index * 44, instance_words=row,
            slot=row[2], sample=row[1], slot_words=slot, volume_bits=slot[5], volume=floats(slot[5:6])[0])
    demand(set(sounds) == set(SOUNDS) and len({row['slot'] for row in sounds.values()}) == 3,
           'Three independently owned original ambient slots were not observed')

    # Xbox32 headers: rf_runtime_events=32; rf_runtime_event=116 bytes,
    # including the appended Alarm_Siren45 active/voice owner fields.
    # Validate actual authored UIDs, type32, generation-bearing registration,
    # and retained Switch state; never infer state from audio diagnostics.
    events = get('campaign_events', 8)
    registry = exact_address(mapping, 'campaign_registry')
    demand(1 <= events[6] <= 1024 and events[2] == events[6] and events[5] == registry and
           events[0] == events[1] and 16 + events[6] * AUTHORED_EVENT_BYTES <= events[3] <= 1024 * 1024 and
           events[7] <= 1024 * 1024 and events[7] >= events[6] * EVENT_BYTES + events[3] + 32,
           'Event owner census/registry/ABI differs')
    runtime = memory(events[4], events[6] * EVENT_WORDS)
    switches = {}
    for index in range(events[6]):
        row = runtime[index * EVENT_WORDS:(index + 1) * EVENT_WORDS]
        if row[2] != 32:
            continue
        demand(row[0] == 6, 'Switch event object type differs')
        demand(row[9] == events[1] + index * AUTHORED_EVENT_BYTES, 'Switch authored pointer is outside its exact owner row')
        uid = memory(row[9], 1)[0]
        if uid not in SWITCHES:
            continue
        pointer, handle = events[4] + index * EVENT_BYTES, row[1]
        demand(uid not in switches and handle not in (0, UINT_MAX) and (handle & 0xffff) < 1024 and row[26] == 0,
               'Switch owner duplicate, retired, or invalid')
        demand(memory(registry + 8 * (handle & 0xffff), 2) == [pointer, handle], 'Stale Switch generation handle')
        demand(events[4] + events[6] * EVENT_BYTES <= row[11] and
               row[11] + 20 <= events[4] + events[7] - events[3] - 32,
               'Switch state outside bounded runtime allocation')
        state = memory(row[11], 5)
        demand(state[0] in (0, 1) and state[1:3] == [0, 1] and state[4] == 0,
               'Original unlimited toggle Switch state differs')
        switches[uid] = dict(uid=uid, pointer=pointer, handle=handle, authored_pointer=row[9],
            state_pointer=row[11], disabled=state[0], activations=state[3], state_words=state)
    demand(set(switches) == set(SWITCHES), 'Missing actual original Switch owners')
    body = get('scene_actor_body', 81)
    position = floats(body[22:25])
    demand(all(math.isfinite(value) for value in position), 'Nonfinite player position')
    demand(math.hypot(position[0] - recipe['original_spawn'][0], position[2] - recipe['original_spawn'][2]) <= 8,
           'Player left bounded original-spawn neighborhood')
    sample = dict(phase=phase, frame=frame, raw=raw, ambient_slots=sounds, switches=switches,
                  player_position=position, transition=get('rf_scene_level_transition', 20),
                  level_transitions=get('rf_xbox_level_transitions', 4))
    audit['complete'] = True
    persist(audit)
    return sample


def evaluate(guest, before, after, recipe):
    demand(before is not None and after is not None, 'Required owner observations missing')
    checks = dict(stock64=guest.get('memory_bytes') == 64 * 1024 * 1024,
        completed=guest.get('guest_phase') == 5 and guest.get('frames') == FRAMES and
                  guest.get('replay_state', [])[1:] == [FRAMES, FRAMES, 0],
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        terminal_alive=guest.get('player_life', [])[:3] == [0, 0, 0],
        terminal_no_transition=guest.get('level_transitions', [None])[0] == 0 and guest.get('level_request', [None])[0] == 0)
    for label, sample in (('before', before), ('after', after)):
        raw = sample['raw']
        checks[label + '_original_spawn'] = raw['rf_scene_player_spawn_diagnostic'][:13] == recipe['spawn_words']
        checks[label + '_no_setup_death_transition_save'] = (raw['rf_scene_setup_result'] == [0] * 4 and
            raw['rf_scene_player_life'][:3] == [0] * 3 and not sample['transition'][0] and
            not sample['level_transitions'][0] and raw['rf_scene_section_autosave'] == [0] * 4)
        checks[label + '_ordinary_controls'] = raw['rf_scene_actor_turn_enabled'] == raw['rf_scene_actor_look_enabled'] == [1]
        checks[label + '_native_audio_open'] = raw['rf_xbox_audio_diagnostic'][0] == 1
    checks['terminal_no_setup_or_save'] = (guest['extra']['rf_scene_setup_result'] == [0] * 4 and
                                         guest['extra']['rf_scene_section_autosave'] == [0] * 4)
    checks['initial_sound_volumes'] = all(before['ambient_slots'][uid]['volume_bits'] == VOLUME_BITS for uid in SOUNDS)
    checks['same_separate_slot_owners'] = all(
        all(before['ambient_slots'][uid][key] == after['ambient_slots'][uid][key]
            for key in ('instance_pointer', 'slot', 'sample', 'instance_words')) for uid in SOUNDS)
    checks['selective4064_off'] = (after['ambient_slots'][4064]['volume_bits'] == 0 and
        all(after['ambient_slots'][uid]['volume_bits'] == VOLUME_BITS for uid in (4062, 4063)))
    checks['initial_switch_states'] = all(before['switches'][uid]['state_words'] == [0, 0, 1, 0, 0] for uid in SWITCHES)
    checks['same_switch_owners'] = all(all(before['switches'][uid][key] == after['switches'][uid][key]
        for key in ('pointer', 'handle', 'authored_pointer', 'state_pointer')) for uid in SWITCHES)
    checks['actual3714_toggled_once'] = after['switches'][3714]['state_words'] == [1, 0, 1, 1, 0]
    checks['other_switches_untouched'] = all(after['switches'][uid]['state_words'] == [0, 0, 1, 0, 0] for uid in (3685, 3916))
    early, late = before['raw']['rf_scene_switch_ambient'], after['raw']['rf_scene_switch_ambient']
    checks['initial_no_slot_dispatch_only'] = early[:5] == [3, 3, 0, 3, 0]
    checks['one_actual_qualified_write'] = late == [4, 4, 1, 3, 0, 4064, after['ambient_slots'][4064]['slot'], 0]
    checks['terminal_no_additional_dispatch'] = guest['extra']['rf_scene_switch_ambient'] == late
    initial_cut, cut = before['raw']['rf_scene_cutscene'], after['raw']['rf_scene_cutscene']
    checks['natural5495_complete'] = (initial_cut[2] == initial_cut[6] == initial_cut[7] == 0 and
        cut[:5] == [1, 4, 1, 5495, 4] and cut[5] == 3 and cut[6:8] == [1, 0] and cut[9] == 0)
    checks['ordinary_trigger_and_audio_processing'] = (after['raw']['rf_scene_trigger_contacts'][1] >
        before['raw']['rf_scene_trigger_contacts'][1] and after['raw']['rf_scene_trigger_contacts'][5] == 0 and
        after['raw']['rf_scene_ambient_audio'][0] > before['raw']['rf_scene_ambient_audio'][0])
    failed = [name for name, passed in checks.items() if not passed]
    audio = {label: {name: sample['raw'][name] for name in
             ('rf_scene_ambient_audio', 'rf_scene_live_audio', 'rf_xbox_audio_diagnostic', 'rf_xbox_music_diagnostic')}
             for label, sample in (('before', before), ('after', after))}
    return dict(status='CHECK_FAILED' if failed else 'PASS_ORIGINAL_SWITCH3714_AMBIENT_OFF',
        checks=checks, failed_checks=failed, native_audio_telemetry=audio,
        audio_warning=bool(after['raw']['rf_scene_ambient_audio'][4] or after['raw']['rf_xbox_audio_diagnostic'][3]),
        limits='Actual slot OFF and unaffected peers; native audio counters are separate, not target-specific audible-output proof. '
               'No live ON for the three switches, save/fresh-load, route, capture or FPS coverage.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--root', type=Path, default=Path('/workspace/shared/OpenRedFaction-attack'))
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--consumer-build', type=Path, required=True,
                        help='Parent-created PASS_XBOX_BUILD manifest with exact source_commit, xbe and map hashes')
    parser.add_argument('--seconds', type=int, default=900)
    args = parser.parse_args()
    if not 120 <= args.seconds <= 1200:
        parser.error('--seconds must be in120..1200')
    root, folder = args.root.resolve(), args.out.resolve()
    if folder.exists() or folder == root or root in folder.parents:
        parser.error('--out must be a new evidence directory outside the repository')
    consumer_path = args.consumer_build.resolve()
    consumer_bytes = consumer_path.read_bytes()
    consumer = json.loads(consumer_bytes)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    source_status = subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True)
    demand(source_status == '', 'Require clean committed parent build source')
    demand(isinstance(consumer, dict) and consumer.get('status') == 'PASS_XBOX_BUILD' and
           consumer.get('source_commit') == head, 'Manifest must identify the current successfully built source')
    for key in ('xbe', 'map'):
        value = consumer.get(key, '')
        demand(isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value),
               'Invalid consumer manifest hash: ' + key)
    sys.path.insert(0, str(root / 'tools'))
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_guest_snapshot import words
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    from inspect_triggers import inspect as inspect_triggers
    demand(native.ROOT.resolve() == root, 'Native runner belongs to another repository')
    disc, iso = root / 'build/xbox/disc', root / 'build/xbox/redfaction-diagnostic.iso'
    xbe, mapping = disc / 'default.xbe', root / 'build/xbox/main.map'
    base = root / 'local/xemu-harness/pacing-base.qcow2'
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (xbe, mapping, base, packer):
        demand(path.is_file(), 'Missing existing parent-built/owned input: ' + str(path))
    demand(sha256(xbe) == consumer['xbe'] and sha256(mapping) == consumer['map'],
           'Existing XBE/map do not match the explicit successful parent build manifest')
    standalone(base)
    map_text = mapping.read_text()
    for name in tuple(SYMBOLS) + OWNER_SYMBOLS:
        exact_address(map_text, name)
    recipe = original_recipe(root, read_entry, inspect_level, inspect_triggers)
    report = dict(status='NOT_RUN', recipe=recipe, observations={}, attempts=0,
        hdd=str(base), hdd_mode='Owned base directly, XEMU -snapshot; no save/load flags or private copy',
        source_commit=head, source_status=source_status, consumer_build=consumer,
        consumer_build_path=str(consumer_path), consumer_build_sha256=hashlib.sha256(consumer_bytes).hexdigest(),
        observer_abi=dict(runtime_event_bytes=EVENT_BYTES, authored_event_bytes=AUTHORED_EVENT_BYTES,
                          authored_pointer_offset=36, switch_pointer_offset=44, retired_offset=104))
    require_no_project_xemu(root)
    lock = SessionLock(root)
    lock.acquire()
    original_lock, original_address, originals = native.SessionLock, native.address, None
    staged = iso_moved = False
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        (folder / 'consumer-build.json').write_bytes(consumer_bytes)
        report['base_hdd_sha256'] = sha256(base)
        report['original_disc_names'] = sorted(path.name for path in disc.iterdir())
        archives = {}
        for path in sorted(disc.iterdir()):
            if not path.is_file() or (path.suffix != '.vpp' and path.name != 'bluebeard.bty') or path.name == 'scene-fixture.vpp':
                continue
            source = root / 'Installed_Game' / path.name
            demand(source.is_file(), 'Non-original disc archive: ' + path.name)
            archives[path.name] = sha256(source)
            demand(sha256(path) == archives[path.name], 'Disc archive differs from original: ' + path.name)
        demand({'levels2.vpp', 'tables.vpp', 'meshes.vpp', 'motions.vpp', 'audio.vpp', 'bluebeard.bty'} <= archives.keys(),
               'Required original assets missing')
        report['original_archive_sha256'] = archives
        immutable = {'geomod-template.bin', 'driller-single.bin', 'driller-double.bin'}
        report['fixed_data_sha256'] = {name: sha256(disc / name) for name in immutable if (disc / name).is_file()}
        names = set(native.FLAGS) | {'scene-fixture.vpp', 'scene-preview.flag', 'player-control.flag', 'audio-output.flag'}
        names |= {path.name for path in disc.iterdir() if path.is_file() and
                  (path.name.startswith('campaign-') or (path.suffix in ('.flag', '.bin', '.txt') and path.name not in immutable))}
        originals = {name: (disc / name).read_bytes() if (disc / name).exists() else None for name in sorted(names)}
        write_json(folder / 'disc-restore.json', {name: data.hex() if data is not None else None for name, data in originals.items()})
        report['exact_inputs'] = dict(xbe=sha256(xbe), map=sha256(mapping), iso=sha256(iso) if iso.exists() else None)
        shutil.copyfile(xbe, folder / 'tested-default.xbe')
        shutil.copyfile(mapping, folder / 'tested-main.map')
        write_json(folder / 'recipe.json', recipe)
        if iso.exists():
            iso.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in originals:
            (disc / name).unlink(missing_ok=True)
        for name in ('campaign-spawn.flag', 'scene-preview.flag', 'player-control.flag', 'audio-output.flag'):
            (disc / name).write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64, b'\0') + b'L7S1.rfl'.ljust(64, b'\0'))
        recording = replay()
        (disc / 'player-replay.bin').write_bytes(recording)
        report['replay_sha256'] = hashlib.sha256(recording).hexdigest()
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        demand(sha256(xbe) == report['exact_inputs']['xbe'] and sha256(mapping) == report['exact_inputs']['map'],
               'Parent-built XBE/map changed before launch')
        demand(sha256(xbe) == consumer['xbe'] and sha256(mapping) == consumer['map'] and
               subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() == head and
               subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True) == '',
               'Clean manifest-pinned build source changed before launch')
        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)
        # Local, temporary substitution keeps the shared runner interface and
        # source unchanged while applying exact-token lookup to its reads too.
        native.address = exact_address

        def probe(phase):
            def read(monitor, current_mapping):
                demand(current_mapping == map_text, 'Guest symbol map changed')
                sample = observe(monitor, current_mapping, words, recipe, phase,
                    lambda audit: write_json(folder / (phase + '-observer-raw.json'), audit))
                report['observations'][phase] = sample
                write_json(folder / (phase + '-owners.json'), sample)
                return sample
            return read

        report['attempts'] = 1
        guest = native.run_guest(folder, 'native', base, FRAMES, args.seconds, snapshot=True,
            extra_symbols=SYMBOLS, allow_guest_error=True, allow_player_dead=True,
            probe=probe('before'), probe_frame=EARLY_FRAME,
            final_probe=probe('after'), final_probe_frame=FINAL_FRAME,
            measure_fps=False, capture_world=False)
        report['native'] = guest
        report.update(evaluate(guest, report['observations'].get('before'), report['observations'].get('after'), recipe))
    except BaseException as error:
        report.update(status='CHECK_FAILED', error=type(error).__name__ + ': ' + str(error))
        raise
    finally:
        native.SessionLock = original_lock
        native.address = original_address
        errors = []
        try:
            if staged:
                for name, data in originals.items():
                    try:
                        # These files were unlinked before staging; do not write
                        # through any pre-existing link to original game inputs.
                        (disc / name).unlink(missing_ok=True)
                        if data is not None:
                            (disc / name).write_bytes(data)
                    except OSError as error:
                        errors.append(name + ': ' + str(error))
            if staged or iso_moved:
                try:
                    iso.unlink(missing_ok=True)
                    if iso_moved:
                        (folder / 'original-disc.iso').rename(iso)
                except OSError as error:
                    errors.append('ISO: ' + str(error))

            def verify(name, operation):
                try:
                    report[name] = bool(operation())
                except (OSError, ValueError, subprocess.SubprocessError) as error:
                    report[name] = False
                    errors.append(name + ': ' + str(error))
                if report[name] is False:
                    errors.append(name + ': verification failed')

            if originals is not None:
                verify('disc_flags_restored', lambda: all(
                    ((disc / name).read_bytes() if (disc / name).exists() else None) == data
                    for name, data in originals.items()))
                verify('disc_names_restored', lambda: sorted(path.name for path in disc.iterdir()) == report['original_disc_names'])
            if 'exact_inputs' in report:
                pins = report['exact_inputs']
                # Normal operation never writes these. Restore from the exact
                # pre-run copies if an unexpected change is detected, then fail
                # the run rather than silently accepting that interference.
                for name, path in (('xbe', xbe), ('map', mapping)):
                    backup = folder / ('tested-default.xbe' if name == 'xbe' else 'tested-main.map')
                    try:
                        if not path.is_file() or sha256(path) != pins[name]:
                            errors.append(name + ': unexpected change; restoring preserved bytes')
                            if backup.is_file() and sha256(backup) == pins[name]:
                                temporary = path.with_name(path.name + '.switch-sound-restore')
                                demand(not temporary.exists(), 'Restore temporary already exists: ' + str(temporary))
                                shutil.copyfile(backup, temporary)
                                temporary.replace(path)
                    except (OSError, RuntimeError) as error:
                        errors.append(name + ': ' + str(error))
                verify('exact_xbe_map_restored', lambda: sha256(xbe) == pins['xbe'] and sha256(mapping) == pins['map'])
                verify('original_iso_restored', lambda: (sha256(iso) if iso.exists() else None) == pins['iso'])
            if 'original_archive_sha256' in report:
                verify('original_archives_unchanged', lambda: all(
                    sha256(disc / name) == digest and sha256(root / 'Installed_Game' / name) == digest
                    for name, digest in report['original_archive_sha256'].items()))
            if 'fixed_data_sha256' in report:
                verify('fixed_data_unchanged', lambda: all(sha256(disc / name) == digest for name, digest in report['fixed_data_sha256'].items()))
            if 'base_hdd_sha256' in report:
                verify('base_hdd_unchanged', lambda: sha256(base) == report['base_hdd_sha256'])
            verify('manifest_source_remains_clean', lambda:
                subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() == head and
                subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True) == '')
            report['restoration_errors'] = errors
            if errors:
                report['status'] = 'CHECK_FAILED'
            if folder.exists():
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != 'PASS_ORIGINAL_SWITCH3714_AMBIENT_OFF':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
