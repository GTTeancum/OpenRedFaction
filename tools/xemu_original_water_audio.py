"""Parent-only 19:00 original L10S3 neutral underwater-owner check.

Source preparation only. One 360-frame all-neutral original-spawn run, after
the parent's successful Xbox build. No build, fixture, grants, direct events,
save/load, route, host input, screenshot, PCM recording, or retry. The existing
native runner uses stock64MiB and the owned base HDD with XEMU -snapshot.
Live, generation-qualified shared/native loop ownership is observed separately
from audibility. Surfacing, dry exit, gain, relocation and teardown are not the
acceptance scope. All observer reads are persisted before their predicates.
"""
import argparse
import hashlib
import io
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
os.environ.setdefault('RF_XEMU_ROOT', '/workspace/shared/xemu-inputs')
os.environ.setdefault('RF_XEMU_BINARY', '/workspace/shared/xemu/squashfs-root/usr/bin/xemu')

from xemu_original_switch_sound import exact_address

RFL_SHA256 = '0268e2acbfb73abacc5fd84b2162eedf14fb91fa55353cd134d0b0a702a49cd4'
GEOMETRY_SHA256 = '3f2287bb387c46cabacdd686007c8c8890b01530b6c1b5db4b6e56f721b4b570'
UNDERWATER_SHA256 = '4778e16983eba6628749bd3364da5b165eee9b1bd5bf7bedca063d1603cedd23'
NX_AUDIO_HEADER_SHA256 = 'a52f170f165a4ad4b5d1eda7a7f781fa97a6af0ece786b7d32f95e5f78f80b3c'
FRAMES, EARLY_FRAME, FINAL_FRAME = 360, 30, 315
UINT_MAX, VOICES = 0xffffffff, 30
# Xbox32 source contracts: physics.h, entity.h, audio.h, scene.c and the pinned
# nxdk-audio header. No guessed retail layouts or suffix-matching symbols.
BODY_WORDS, SAMPLE_WORDS, MIXER_VOICE_WORDS = 81, 28, 13
NATIVE_SLOT_WORDS, STREAM_PREFIX_WORDS, ROOM_CACHE_WORDS = 55, 44, 12
SYMBOLS = {
    'rf_scene_player_water_audio': 16, 'rf_scene_player_water_audio_values': 2,
    'rf_scene_player_swim': 12, 'rf_scene_player_room_cache': 6,
    'rf_scene_cutscene': 12, 'rf_scene_vehicle_state': 16,
    'rf_scene_turret_player': 12, 'rf_scene_live_audio': 8,
    'rf_scene_controller_audio': 4, 'rf_xbox_audio_diagnostic': 12,
    'rf_xbox_music_diagnostic': 12, 'rf_scene_player_spawn_diagnostic': 19,
    'rf_scene_setup_result': 4, 'rf_scene_player_life': 8,
    'rf_scene_section_autosave': 4, 'rf_scene_script_grants': 8,
    'rf_scene_actor_turn_enabled': 1, 'rf_scene_actor_look_enabled': 1,
}
OWNER_SYMBOLS = (
    'scene_actor_body', 'scene_actor_collision_owner', 'scene_player_water_audio',
    'campaign_player_object', 'campaign_player_view', 'campaign_player_damage',
    'campaign_registry', 'campaign_entities', 'campaign_listener_position',
    'campaign_audio_bank', 'campaign_audio_mixer', 'campaign_spatial_voices',
    'campaign_device_voice_ids', 'campaign_audio_events', 'rf_xbox_audio_events', 'slots',
)


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def floats(value):
    return list(struct.unpack('<' + 'f' * len(value), struct.pack('<' + 'I' * len(value), *value)))


def replay():
    return b'RFI6' + struct.pack('<I', 48) + bytes(48 * FRAMES)


def original_recipe(root, read_entry, inspect_level):
    from inspect_geometry import inspect as inspect_geometry
    from inspect_liquid_geometry import liquid
    data = read_entry(root / 'Installed_Game/levels2.vpp', 'L10S3.rfl')
    demand(hashlib.sha256(data).hexdigest() == RFL_SHA256, 'Original L10S3 hash differs')
    meta = inspect_level(io.BytesIO(data), dict(offset=0, size=len(data), name='L10S3.rfl'))
    demand(meta['version'] == 180, 'Require original version180')

    def section(kind):
        rows = [row for row in meta['sections'] if row['type'] == hex(kind)]
        demand(len(rows) == 1, 'Missing/ambiguous original section')
        row = rows[0]
        return data[row['offset'] + 8:row['offset'] + 8 + row['size']]

    start = list(struct.unpack('<12I', section(0x70000)))
    geometry = section(0x100)
    demand(hashlib.sha256(geometry).hexdigest() == GEOMETRY_SHA256, 'Original geometry hash differs')
    geometry_meta = inspect_geometry(geometry)
    rows = [row for row in liquid(geometry)['liquid_rooms'] if row['index'] == 19]
    demand(len(rows) == 1, 'Missing original liquid room19')
    room = rows[0]
    minimum_y = room['bounds'][1]
    depth = struct.unpack_from('<f', bytes.fromhex(room['prefix_hex']))[0]
    liquid_type = struct.unpack_from('<I', bytes.fromhex(room['tail_hex']), 4)[0]
    demand(struct.unpack_from('<I', geometry, 1911)[0] == 6645 and
           minimum_y == -80.10407257080078 and depth == 30.0 and liquid_type == 1 and
           room['texture'] == 'greenwater01.vbm', 'Original room19/UID6645 water record differs')
    spawn = floats(start[:3])
    demand(all(room['bounds'][i] < spawn[i] < room['bounds'][i + 3] for i in range(3)) and
           spawn[1] < minimum_y + depth - 10, 'Authored spawn is not deeply inside the wet room bounds')
    source = read_entry(root / 'Installed_Game/audio.vpp', 'Underwater_01.wav')
    demand(len(source) == 83776 and hashlib.sha256(source).hexdigest() == UNDERWATER_SHA256,
           'Original underwater sample differs')
    header = root / 'build/nxaudio/include/nxaudio.h'
    demand(hashlib.sha256(header.read_bytes()).hexdigest() == NX_AUDIO_HEADER_SHA256,
           'Native voice ABI header differs; re-review the observer before any run')
    return dict(level='L10S3.rfl', archive='levels2.vpp', rfl_sha256=RFL_SHA256,
        geometry_sha256=GEOMETRY_SHA256, rfl_bytes=len(data),
        spawn_words=[1] + start[:3] + start[6:12] + start[3:6], original_spawn=spawn,
        room=19, room_uid=6645, room_count=geometry_meta['rooms'], room_bounds=room['bounds'],
        liquid_words=list(struct.unpack('<3I', struct.pack('<2fI', minimum_y, depth, liquid_type))),
        liquid_surface=minimum_y + depth, liquid_texture=room['texture'],
        underwater_sha256=UNDERWATER_SHA256, underwater_file_bytes=len(source),
        underwater_pcm=[83732, 41866, 11025, 1, 16],
        native_header_sha256=NX_AUDIO_HEADER_SHA256,
        frames=FRAMES, early_probe=EARLY_FRAME, final_probe=FINAL_FRAME,
        input='RFI6, 360 records of 48 zero bytes. Every movement, look and button field is zero.',
        static_membership='Authored room19 front-facing face2441 is the nearest original room-query ray intersection; '
                          'opposite face2444 belongs to room18. Source-only geometric evidence, not guest execution.',
        scope='Underwater startup and same-generation loop reuse at natural original spawn; surface loop remains absent.',
        limits='No surfacing/dry-stop, speed gain, relocation, vehicle/cinematic audio, save/load, audibility or FPS claim.')


def observe(monitor, mapping, words, recipe, phase, persist):
    audit = dict(phase=phase, complete=False, reads=[])
    persist(audit)

    def read_words(address, count, symbol=None):
        demand(isinstance(address, int) and 0x10000 <= address <= 0xffffffff and address % 4 == 0 and
               0 < count <= 4096 and address + count * 4 <= 0x100000000,
               'Invalid/beyond-bound guest pointer read')
        entry = dict(address=address, count=count, symbol=symbol, status='REQUESTED', words=[])
        audit['reads'].append(entry)
        persist(audit)
        try:
            for offset in range(0, count, 256):
                part = words(monitor, address + offset * 4, min(256, count - offset))
                entry['words'].extend(part)
                persist(audit)
                demand(len(part) == min(256, count - offset), 'Incomplete guest memory read')
        except BaseException as error:
            entry.update(status='READ_FAILED', error=type(error).__name__ + ': ' + str(error))
            persist(audit)
            raise
        entry['status'] = 'READ'
        persist(audit)
        return entry['words']

    def get(name, count):
        return read_words(exact_address(mapping, name), count, name)

    diagnostic = get('rf_diagnostic', 58)
    frame = diagnostic[37]
    audit['frame'] = frame
    persist(audit)
    demand(diagnostic[2] == 2, 'Owner probe did not occur in the live scene')
    demand(EARLY_FRAME <= frame < 80 if phase == 'before' else FINAL_FRAME <= frame < FRAMES,
           'Missed required live probe window: ' + phase + ' frame' + str(frame))
    raw = {name: get(name, count) for name, count in SYMBOLS.items()}
    owner = get('scene_player_water_audio', 7)
    body = get('scene_actor_body', BODY_WORDS)
    listener = get('campaign_listener_position', 3)
    player = get('campaign_player_object', 3)
    view = get('campaign_player_view', 15)
    damage = get('campaign_player_damage', 14)
    stream_pointer = get('scene_actor_collision_owner', 1)[0]
    stream = read_words(stream_pointer, STREAM_PREFIX_WORDS, 'scene_stream prefix')
    bank = get('campaign_audio_bank', 6)
    ids = get('campaign_device_voice_ids', VOICES * 2 + 1)
    callbacks = get('campaign_audio_events', 9)
    native_callbacks = get('rf_xbox_audio_events', 9)
    transition = get('rf_scene_level_transition', 20)
    transitions = get('rf_xbox_level_transitions', 4)
    # Persist every top-level owner before examining any of its identities.
    water = raw['rf_scene_player_water_audio']
    demand(raw['rf_scene_player_spawn_diagnostic'][:13] == recipe['spawn_words'], 'Original spawn/basis changed')
    demand(body[80] >= BODY_WORDS * 4 and 0 < body[78] <= 128,
           'Live player physics owner/ABI differs')
    handle = player[1]
    view_pointer = exact_address(mapping, 'campaign_player_view')
    demand(player[0] == 0 and handle not in (0, UINT_MAX) and (handle & 0xffff) < 1024 and
           player[2] == view_pointer and view[0] == handle and view[1] == 0 and
           view[3] & 8 and view[7] == UINT_MAX and damage[4] == handle and
           math.isfinite(floats(damage[:1])[0]) and floats(damage[:1])[0] > 0,
           'Missing living, registered, on-foot local-player owner')
    entity_slot = read_words(exact_address(mapping, 'campaign_entities') + 4 * (handle & 0xffff), 1,
                             'player entity-registry slot')
    object_slot = read_words(exact_address(mapping, 'campaign_registry') + 8 * (handle & 0xffff), 2,
                             'player object-registry slot')
    demand(entity_slot == [view_pointer] and object_slot == [exact_address(mapping, 'campaign_player_object'), handle],
           'Stale player generation-qualified registry identity')
    demand(owner[4] == handle and owner[6] == 1 and owner[5] == water[15] and
           max(0, frame - 2) <= owner[5] <= frame + 1,
           'Water owner is stale or does not belong to the actual player')
    demand(water[1] > 0 and water[2:9] == [0, 1, 0, 0, 0, 0, 0] and water[9:11] == [1, 1] and
           owner[2] == water[13] == UINT_MAX and owner[3] == water[14] and owner[1] == water[12],
           'Expected one clean underwater start, no surface voice and no stop/reacquisition')
    demand(raw['rf_scene_cutscene'][2] == raw['rf_scene_cutscene'][7] == 0 and
           raw['rf_scene_vehicle_state'][1:4] == [0, 0, 0] and
           raw['rf_scene_turret_player'][:3] == [0, 0, 0], 'Unexpected cinematic or occupied vehicle/turret')
    position, eye = floats(body[22:25]), floats(listener)
    demand(all(math.isfinite(value) for value in position + eye), 'Nonfinite actual body/gameplay eye position')
    demand(stream[11] == recipe['room_count'] and stream[12] == stream[11] * 12,
           'Liquid owner census/allocation differs')
    caches = [stream[20:32], stream[32:44]]
    demand(caches[0][0:2] == caches[1][0:2] and all(row[5] == 1 and row[9] == recipe['room'] for row in caches),
           'Actual body/camera query caches do not select original water room19')
    demand(caches[0][6:9] == body[22:25] and caches[1][6:9] == listener,
           'Body/camera room caches refer to different live positions')
    world = read_words(caches[0][0], 18, 'exact room-cache collision world')
    liquid = read_words(stream[10] + recipe['room'] * 12, 3, 'actual room19 liquid owner')
    marker_address = world[6] + recipe['room']
    marker = read_words(marker_address & ~3, 1, 'authored contains-liquid byte owner')
    demand(world[2] == caches[0][1] and world[7] == recipe['room_count'] and
           ((marker[0] >> (8 * (marker_address & 3))) & 255) == 1 and liquid == recipe['liquid_words'],
           'Published exact-query world or original liquid record differs')
    height = floats(liquid[:1])[0] + floats(liquid[1:2])[0]
    demand(position[1] <= height and eye[1] <= height and
           raw['rf_scene_player_swim'][:4] == [19, 1, 1, 4] and raw['rf_scene_player_swim'][11] == 0,
           'Actual body/gameplay eye are not both underwater in ordinary swimming mode')
    demand(callbacks == native_callbacks and callbacks[5] != 0 and raw['rf_xbox_audio_diagnostic'][0] == 1,
           'Native Xbox play-mode backend is not actually bound/open')
    sample_index, voice_id = owner[1], owner[3]
    demand(0 <= sample_index < bank[2] <= bank[3] <= 4096 and 0 < bank[4] <= bank[5] <= 1280 * 1024 and
           voice_id < 0x80000000, 'Invalid bounded sample-bank or water voice identity')
    matches = [i for i in range(VOICES) if ids[i] == voice_id]
    demand(len(matches) == 1, 'Underwater device identity does not resolve uniquely')
    slot = matches[0]
    source_handle = ids[VOICES + slot]
    mixer = read_words(exact_address(mapping, 'campaign_audio_mixer') + slot * MIXER_VOICE_WORDS * 4,
                       MIXER_VOICE_WORDS, 'underwater shared mixer voice')
    spatial = read_words(exact_address(mapping, 'campaign_spatial_voices') + slot * 36, 9,
                         'underwater flat spatial owner')
    sample = read_words(bank[1] + sample_index * SAMPLE_WORDS * 4, SAMPLE_WORDS,
                        'underwater bank sample metadata only')
    name_bytes = struct.pack('<16I', *sample[:16])[:61]
    demand(b'\0' in name_bytes and name_bytes.split(b'\0')[0].lower() == b'underwater_01.wav' and
           sample[16] >= 0x10000 and sample[17] >= sample[16] and
           sample[17] + sample[18] <= sample[16] + sample[23] and
           sample[18:23] == recipe['underwater_pcm'] and sample[23] == recipe['underwater_file_bytes'],
           'Voice is not backed by the original resident underwater PCM metadata')
    demand(source_handle != 0 and (source_handle & 0xffff) == slot and (source_handle >> 16) != 0 and
           mixer[:6] == sample[17:23] and mixer[6] == source_handle and mixer[11:13] == [1, 1] and
           mixer[7] < mixer[2] and spatial[0:2] == [source_handle, sample_index] and spatial[8] == 0,
           'Stale, stopped, non-looping, wrong-sample or positional shared voice')
    native_table = get('slots', VOICES * NATIVE_SLOT_WORDS)
    native_matches = [i for i in range(VOICES) if
                      native_table[i * NATIVE_SLOT_WORDS + 53:i * NATIVE_SLOT_WORDS + 55] == [source_handle, 1]]
    demand(len(native_matches) == 1, 'No unique native Xbox voice owns the full shared generation handle')
    native_slot = native_matches[0]
    native_pointer = exact_address(mapping, 'slots') + native_slot * NATIVE_SLOT_WORDS * 4
    native = native_table[native_slot * NATIVE_SLOT_WORDS:(native_slot + 1) * NATIVE_SLOT_WORDS]
    demand(native[0:5] == [0, 11025, 0x0201, 0, 0] and native[19] & 0xffff == 1 and
           native[51:53] == sample[17:19] and native[5] == native_pointer + 204,
           'Native source is not PLAYING mono PCM16 11025Hz/static/looping with the exact sample borrower')
    result = dict(phase=phase, frame=frame, water_frame=owner[5], raw=raw,
        player_handle=handle, player_position=position, gameplay_eye=eye,
        displacement_from_authored_spawn=math.dist(position, recipe['original_spawn']),
        stream_pointer=stream_pointer, room_caches=caches, liquid_words=liquid,
        sample_index=sample_index, sample_words=sample, voice_id=voice_id,
        source_handle=source_handle, mixer_slot=slot, mixer_words=mixer, spatial_words=spatial,
        native_slot=native_slot, native_pointer=native_pointer, native_words=native,
        transition=transition, level_transitions=transitions)
    audit['complete'] = True
    persist(audit)
    return result


def evaluate(guest, before, after, recipe):
    demand(before is not None and after is not None, 'Required live owner observations missing')
    checks = dict(stock64=guest.get('memory_bytes') == 64 * 1024 * 1024,
        completed=guest.get('guest_phase') == 5 and guest.get('frames') == FRAMES and
                  guest.get('replay_state', [])[1:] == [FRAMES, FRAMES, 0],
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        terminal_alive=guest.get('player_life', [])[:3] == [0, 0, 0],
        terminal_no_transition=guest.get('level_transitions', [None])[0] == 0 and guest.get('level_request', [None])[0] == 0,
        beyond_one_source_duration=(after['water_frame'] - before['water_frame']) / 60 > 41866 / 11025,
        same_generation_owners=all(before[key] == after[key] for key in
            ('player_handle', 'stream_pointer', 'sample_index', 'voice_id', 'source_handle', 'mixer_slot',
             'native_slot', 'native_pointer', 'sample_words')))
    for label, sample in (('before', before), ('after', after)):
        raw = sample['raw']
        checks[label + '_no_setup_death_transition_save_grant'] = (
            raw['rf_scene_setup_result'] == [0] * 4 and raw['rf_scene_player_life'][:3] == [0] * 3 and
            not sample['transition'][0] and not sample['level_transitions'][0] and
            raw['rf_scene_section_autosave'] == [0] * 4 and raw['rf_scene_script_grants'] == [0] * 8)
        checks[label + '_ordinary_controls'] = raw['rf_scene_actor_turn_enabled'] == raw['rf_scene_actor_look_enabled'] == [1]
    terminal = guest['extra']
    checks['terminal_no_setup_save_grant'] = (terminal['rf_scene_setup_result'] == [0] * 4 and
        terminal['rf_scene_section_autosave'] == [0] * 4 and terminal['rf_scene_script_grants'] == [0] * 8 and
        guest['checkpoint_state'][8:10] == [0, 0])
    water = terminal['rf_scene_player_water_audio']
    checks['terminal_one_underwater_start_no_surface_or_failure'] = (
        water[2:4] == [0, 1] and water[5:9] == [0, 0, 0, 0])
    checks['native_no_shutdown_fault'] = terminal['rf_xbox_audio_diagnostic'][11] == 0
    failed = [name for name, passed in checks.items() if not passed]
    return dict(status='CHECK_FAILED' if failed else 'PASS_ORIGINAL_UNDERWATER_OWNER', checks=checks,
        failed_checks=failed, native_audio_warning=bool(terminal['rf_xbox_audio_diagnostic'][3]),
        limits=recipe['limits'] + ' Native PLAYING/loop flags and exact PCM borrower are owner evidence; '
               'no waveform capture, listening, APU-cursor progress or gap-free audio claim.')


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
    recipe = original_recipe(root, read_entry, inspect_level)
    report = dict(status='NOT_RUN', recipe=recipe, observations={}, attempts=0,
        hdd=str(base), hdd_mode='Owned base directly, XEMU -snapshot; no save/load flags or private copy',
        source_commit=head, source_status=source_status, consumer_build=consumer,
        consumer_build_path=str(consumer_path), consumer_build_sha256=hashlib.sha256(consumer_bytes).hexdigest(),
        observer_abi=dict(body_bytes=BODY_WORDS * 4, sample_bytes=SAMPLE_WORDS * 4,
                          mixer_voice_bytes=MIXER_VOICE_WORDS * 4, native_slot_bytes=NATIVE_SLOT_WORDS * 4,
                          stream_prefix_bytes=STREAM_PREFIX_WORDS * 4, room_cache_bytes=ROOM_CACHE_WORDS * 4))
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
        (disc / 'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64, b'\0') + b'L10S3.rfl'.ljust(64, b'\0'))
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
                                temporary = path.with_name(path.name + '.water-audio-restore')
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
    if report['status'] != 'PASS_ORIGINAL_UNDERWATER_OWNER':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
