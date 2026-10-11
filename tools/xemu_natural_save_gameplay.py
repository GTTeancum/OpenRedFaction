"""Parent-only original L3S1 normal live save -> fresh load -> bounded controls.

No build, fixture, guest-state write, grant, forced event, host input, image or
performance measurement. A new genuine producer HDD is retained separately
from the missing historical L11S3 regression. Parent hourly batch only.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from types import MappingProxyType

sys.dont_write_bytecode = True
os.environ.setdefault('RF_XEMU_ROOT', '/workspace/shared/xemu-inputs')
os.environ.setdefault('RF_XEMU_BINARY', '/workspace/shared/xemu/squashfs-root/usr/bin/xemu')
SAVE_FRAMES, LOAD_FRAMES, SAVE_AT = 180, 240, 120
MOVE_BEGIN, MOVE_END, FIRE_AT = 120, 135, 180
UINT_MAX = 0xffffffff
BASE_HDD_SHA256 = 'b5df1ad52a7814e14dbcf5d516c4267f654eb5bacd64970642c0ba03a75b7485'
LIMITS = ('NEW original L3S1 live save and fresh-process ordinary load; bounded '
          '15-frame forward input and one primary press. Separate movement/shot '
          'observations, no promised contact, route, authored mission inventory, '
          'physical controller, audio, image, performance or missing old L11S3 regression claim.')
SYMBOLS = {
    'rf_scene_player_spawn_diagnostic': 19, 'rf_scene_weapon_supply': 4,
    'rf_scene_setup_result': 4, 'rf_scene_script_grants': 8,
    'rf_scene_section_autosave': 4, 'rf_scene_player_ammo': 8,
    'rf_scene_combat': 8, 'rf_scene_pickups': 8,
    'rf_scene_enemy_actor_hits': 113,
    'rf_scene_weapon_selection': 8, 'rf_scene_world_load_reject': 3,
    'rf_scene_world_restore_reject': 8, 'rf_scene_npc_restore_reject': 6,
    'rf_scene_npc_checkpoint_reject_state': 6,
    'rf_scene_npc_checkpoint_velocity_observation': 128,
    'rf_scene_npc_checkpoint_airborne_observation': 16,
    'rf_scene_world_snapshot_event_probe': 6,
}
LIVE_SYMBOLS = {
    'scene_actor_body': 52, 'campaign_player_inventory': 112,
    'campaign_pistol_id': 1, 'campaign_equipped_slot': 1,
    'campaign_explicit_unarmed': 1, 'rf_scene_actor_landing': 8,
    'rf_scene_player_ammo': 8, 'rf_scene_combat': 8,
    'rf_scene_pickups': 8, 'rf_scene_script_grants': 8,
    'rf_scene_world_checkpoint_state': 10,
    'rf_xbox_checkpoint_storage_state': 8,
    'rf_scene_actor_ring_frames': 64, 'rf_scene_actor_input_frames': 192,
}


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def floats(values):
    decoded = struct.unpack('<' + 'f'*len(values), struct.pack('<' + 'I'*len(values), *values))
    return [v if math.isfinite(v) else None for v in decoded]


def recording(consumer=False):
    count = LOAD_FRAMES if consumer else SAVE_FRAMES
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, int(consumer and MOVE_BEGIN <= i < MOVE_END),
                    0, 0, 0, 0, 0, int(consumer and i == FIRE_AT), 0, 0, 0)
        for i in range(count))


def live_reader(folder, name, lower, upper, words, exact_address, write_json):
    def read(monitor, mapping):
        diagnostic = words(monitor, exact_address(mapping, 'rf_diagnostic'), 58)
        row = dict(frame=diagnostic[37], phase=diagnostic[2], raw={})
        write_json(folder / (name + '.json'), row)
        demand(row['phase'] == 2 and lower <= row['frame'] < upper,
               name + ': missed required live sampling window')
        row['raw'] = {key: words(monitor, exact_address(mapping, key), count)
                      for key, count in LIVE_SYMBOLS.items()}
        body = row['raw']['scene_actor_body']
        row['position'] = floats(body[22:25])
        row['velocity'] = floats(body[46:49])
        inventory = struct.pack('<112I', *row['raw']['campaign_player_inventory'])
        row['inventory_hex'] = inventory.hex()
        row['selected'] = dict(pistol_id=row['raw']['campaign_pistol_id'][0],
            slot=row['raw']['campaign_equipped_slot'][0],
            explicit_unarmed=row['raw']['campaign_explicit_unarmed'][0],
            pistol_owned=inventory[3], pistol_loaded=struct.unpack_from('<i', inventory, 204)[0])
        write_json(folder / (name + '.json'), row)
        demand(all(v is not None for v in row['position'] + row['velocity']),
               name + ': nonfinite body position/velocity')
        return row
    return read


def decode_actual_payload(path):
    """Read-only structure inspection; native load remains semantic authority."""
    payload = path.read_bytes()
    demand(320 <= len(payload) <= 110524 and payload[:4] == b'RFWC', 'Missing actual RFWC')
    version, total, checksum, count, reserved = struct.unpack_from('<5I', payload, 4)
    demand(version in (2, 3) and count == (16 if version == 2 else 17) and
           total == len(payload) and not reserved and not any(payload[120:128]), 'Invalid RFWC header')
    value = 2166136261
    for index, byte in enumerate(payload):
        value = ((value ^ (0 if 12 <= index < 16 else byte)) * 16777619) & UINT_MAX
    demand(value == checksum, 'RFWC checksum mismatch')
    demand(payload[56:120] == b'L3S1.rfl'.ljust(64, b'\0'), 'Saved level differs from L3S1')
    at, player = 128 + count*12, None
    for index in range(count):
        kind, offset, size = struct.unpack_from('<3I', payload, 128 + 12*index)
        demand(kind == index+1 and offset == at and size <= len(payload)-at, 'Invalid RFWC directory')
        if kind == 1:
            player = payload[at:at+size]
        at += size
    demand(at == len(payload) and player is not None and len(player) == 544 and
           player[:4] == b'RFPL', 'Missing ordinary player component')
    demand(struct.unpack_from('<I', player, 4)[0] in (1, 2, 4), 'Expected standing RFPL')
    position = list(struct.unpack_from('<3f', player, 32))
    velocity = list(struct.unpack_from('<3f', player, 68))
    demand(all(math.isfinite(v) for v in position+velocity), 'Invalid saved body scalars')
    return dict(bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest(), version=version,
                identity=payload[24:56].hex(), level='L3S1.rfl', position=position, velocity=velocity,
                weapon=struct.unpack_from('<I', player, 20)[0], inventory_hex=player[80:528].hex())


def common_checks(guest, frames, recipe):
    extra = guest.get('extra', {})
    return dict(stock64=guest.get('memory_bytes') == 64*1024*1024,
        terminal_complete=guest.get('guest_phase') == 5 and guest.get('frames') == frames,
        exact_replay=guest.get('replay_state', [])[1:] == [frames, frames, 0] and
                     guest.get('actor_frame_count') == frames,
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        no_death_or_respawn=guest.get('player_life', [])[:3] == [0, 0, 0],
        no_transition=guest.get('level_transitions', [None])[0] == 0 and
                      guest.get('level_request', [None])[0] == 0,
        original_spawn=extra.get('rf_scene_player_spawn_diagnostic', [])[:13] == recipe['spawn_words'],
        no_setup=extra.get('rf_scene_setup_result') == [0]*4,
        no_grants=extra.get('rf_scene_script_grants') == [0]*8,
        no_section_autosave=extra.get('rf_scene_section_autosave') == [0]*4)


def producer_checks(guest, recipe, payload, shutdown):
    checks = common_checks(guest, SAVE_FRAMES, recipe)
    state, storage, extra = guest['checkpoint_state'], guest['storage_state'], guest['extra']
    initial = guest.get('probe', {})
    raw, selected = initial.get('raw', {}), initial.get('selected', {})
    checks.update(live_prefire_inventory=selected == dict(pistol_id=3, slot=0, explicit_unarmed=0,
                                                        pistol_owned=1, pistol_loaded=16),
        original_start_supply=raw.get('rf_scene_player_ammo') == [3,125,16,0,0,0,448,0],
        no_prior_request=raw.get('rf_scene_world_checkpoint_state') == [0]*10 and
                         raw.get('rf_xbox_checkpoint_storage_state') == [0]*8,
        no_player_shot=extra['rf_scene_combat'][0] == 0,
        live_save_only=state[8:] == [0,1] and state[3] == 0 and state[4] == payload['bytes'] and
                       state[5] > 0 and state[6] in (0,1),
        normal_pistol_saved=payload['weapon'] == 3 and payload['inventory_hex'] == initial.get('inventory_hex'),
        native_flush_closed=storage[1] == 0 and storage[4:7] == [state[5],state[6],state[4]] and
                            storage[7] == 4,
        strict_shutdown=shutdown.get('clean') is True and shutdown.get('qmp_quit_acknowledged') is True and
                        shutdown.get('returncode') == 0 and shutdown.get('forced_cleanup_used') is False)
    return checks


def consumer_checks(guest, recipe, saved, producer, shutdown):
    checks = common_checks(guest, LOAD_FRAMES, recipe)
    state, storage, extra = guest['checkpoint_state'], guest['storage_state'], guest['extra']
    pre, moved = guest.get('probe', {}), guest.get('final_probe', {})
    a, b = pre.get('raw', {}), moved.get('raw', {})
    end_ammo, end_combat = extra['rf_scene_player_ammo'], extra['rf_scene_combat']
    checks.update(fresh_native_load=state[8:] == [1,0] and state[0] == 0 and
                  state[1] == saved['bytes'] and state[2] > 0,
        same_saved_generation=storage[1] == 0 and storage[4:7] == producer['storage_state'][4:7],
        no_load_reject=extra['rf_scene_world_load_reject'] == [0]*3 and
                       extra['rf_scene_world_restore_reject'][1] == 0 and
                       extra['rf_scene_npc_restore_reject'][3] == 0,
        restored_inventory=pre.get('inventory_hex') == saved['inventory_hex'],
        owned_selected_pistol=pre.get('selected') == dict(pistol_id=3,slot=0,explicit_unarmed=0,pistol_owned=1,pistol_loaded=16),
        strict_shutdown=shutdown.get('clean') is True and shutdown.get('qmp_quit_acknowledged') is True and
                        shutdown.get('returncode') == 0 and shutdown.get('forced_cleanup_used') is False)
    positions = pre.get('position', []) + moved.get('position', [])
    position_valid = len(positions) == 6 and all(v is not None and math.isfinite(v) for v in positions)
    displacement = sum((x-y)**2 for x,y in zip(pre.get('position', []), moved.get('position', []))) if position_valid else None
    horizontal_displacement = sum((pre['position'][i]-moved['position'][i])**2 for i in (0,2)) if position_valid else None
    rings, inputs = b.get('rf_scene_actor_ring_frames', []), b.get('rf_scene_actor_input_frames', [])
    tagged_move = all(len(rings) == 64 and rings[i%64] == i and len(inputs) == 192 and
        floats(inputs[(i%64)*3:(i%64)*3+3]) == [0.0,0.0,1.0] for i in range(MOVE_BEGIN,MOVE_END))
    checks['movement_observed'] = position_valid and horizontal_displacement > .0001 and tagged_move
    checks['one_actual_shot'] = bool(a and b) and a['rf_scene_combat'][0] == 0 and b['rf_scene_combat'][0] == 0 and end_combat[0] == 1
    checks['one_round_consumed'] = bool(a and b) and a['rf_scene_player_ammo'] == b['rf_scene_player_ammo'] and \
        a['rf_scene_player_ammo'] == [3,125,16,0,0,0,448,0] and end_ammo == [3,125,15,0,0,0,448,0]
    checks['no_intervening_pickup'] = bool(a and b) and \
        a['rf_scene_pickups'][3:5] == b['rf_scene_pickups'][3:5] == extra['rf_scene_pickups'][3:5] == [0,0] and \
        a['rf_scene_pickups'][7] == b['rf_scene_pickups'][7] == extra['rf_scene_pickups'][7] == 0
    return checks, dict(before=pre.get('position'), after=moved.get('position'), displacement_squared=displacement,
                        horizontal_displacement_squared=horizontal_displacement,
                        tagged_movement_input=tagged_move, actual_shots=end_combat[0], actual_hits=end_combat[1], actual_kills=end_combat[2])


def read_producer_evidence(folder, head, build, sha256):
    """Read exact producer provenance for an already admitted source identity."""
    folder = folder.resolve()
    verification_path = folder / 'verification.json'
    manifest_path = folder / 'producer-immutable.json'
    verification = json.loads(verification_path.read_text())
    manifest = json.loads(manifest_path.read_text())
    demand(verification.get('status') == 'PASS_NEW_NATIVE_SAVE_AWAITING_DURABLE_BACKUP' and
           verification.get('producer_promoted') is True and
           verification.get('restoration_errors') == [] and
           all(verification.get('producer_checks', {}).values()) and
           verification.get('producer_checks'), 'Producer did not complete verified preservation')
    demand(verification.get('source_commit') == head and manifest.get('source_commit') == head and
           manifest.get('xbe') == build['xbe'] and manifest.get('map') == build['map'],
           'Resume requires the exact producer source/XBE/map')
    demand(manifest.get('purpose') == 'NEW_L3S1_NATURAL_SAVE' and
           manifest.get('historical_regression_recovered') is False and
           manifest.get('read_only') is True, 'Unexpected producer provenance')
    disk, backup = folder / 'new-l3s1-producer.qcow2', folder / 'new-l3s1-immutable-backup.qcow2'
    demand(manifest.get('path') == str(disk) and manifest.get('backup') == str(backup),
           'Producer paths differ from manifest')
    digest, size = manifest.get('sha256'), manifest.get('bytes')
    for path in (disk, backup):
        demand(path.is_file() and not path.stat().st_mode & 0o222 and
               path.stat().st_size == size and sha256(path) == digest,
               'Missing, writable or changed producer/backup HDD: ' + str(path))
    producer = verification['producer']
    shutdown = json.loads((folder / 'producer/shutdown.json').read_text())
    demand(shutdown == manifest.get('shutdown') == verification.get('producer_shutdown') and
           shutdown.get('clean') is True and shutdown.get('returncode') == 0 and
           shutdown.get('qmp_quit_acknowledged') is True and not shutdown.get('forced_cleanup_used'),
           'Missing exact clean producer shutdown')
    saved = decode_actual_payload(folder / 'producer/xbox-world.rfwc')
    demand(saved == verification.get('saved_payload') and saved['sha256'] == manifest.get('payload_sha256') and
           producer['checkpoint_state'][4:7] == [saved['bytes'], manifest.get('generation'), manifest.get('slot')],
           'Producer payload/slot evidence changed')
    return dict(folder=folder, disk=disk, backup=backup, verification=verification,
                manifest=manifest, saved=saved, producer=producer,
                verification_sha256=sha256(verification_path), manifest_sha256=sha256(manifest_path))


def admit_producer(folder, receipt_path, head, build, sha256):
    """Original V1 admission. Current source equality and raw receipt stay required."""
    admitted = read_producer_evidence(folder, head, build, sha256)
    folder, manifest = admitted['folder'], admitted['manifest']
    digest, size = manifest['sha256'], manifest['bytes']
    manifest_path = folder / 'producer-immutable.json'
    receipt_bytes = receipt_path.read_bytes()
    receipt = json.loads(receipt_bytes)
    demand(receipt.get('kind') == 'RF_NATURAL_SAVE_DURABLE_BACKUP_V1' and
           receipt.get('remote_upload_verified_by_parent') is True and
           receipt.get('producer_hdd_sha256') == digest and receipt.get('producer_hdd_bytes') == size and
           receipt.get('producer_manifest_sha256') == sha256(manifest_path),
           'Missing parent-verified durable backup of the exact producer')
    artifacts = receipt.get('artifacts')
    demand(isinstance(artifacts, list) and 1 <= len(artifacts) <= 64, 'Missing durable Library artifacts')
    for artifact in artifacts:
        demand(isinstance(artifact, dict) and isinstance(artifact.get('library_file_id'), str) and
               artifact['library_file_id'].startswith('libfile_') and
               isinstance(artifact.get('version'), int) and artifact['version'] >= 0 and
               isinstance(artifact.get('sha256'), str) and len(artifact['sha256']) == 64 and
               all(c in '0123456789abcdef' for c in artifact['sha256']), 'Invalid durable artifact receipt')
    demand(not (folder / 'consumer-attempt.json').exists(), 'Consumer attempt already reserved; no automatic retry')
    admitted.update(receipt=receipt, receipt_sha256=hashlib.sha256(receipt_bytes).hexdigest())
    return admitted


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--phase', choices=('producer', 'consumer'), required=True)
    parser.add_argument('--producer-dir', type=Path, help='Exact completed producer evidence; consumer only')
    parser.add_argument('--durable-backup-receipt', type=Path, help='Parent-verified Library backup receipt; consumer only')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--consumer-build', type=Path, required=True,
                        help='PASS_XBOX_BUILD manifest with exact source_commit, xbe and map hashes')
    parser.add_argument('--seconds', type=int, default=600)
    parser.add_argument('--continuation-contract', type=Path,
                        help='Explicit reviewed consumer-only compatibility contract')
    parser.add_argument('--continuation-contract-sha256')
    parser.add_argument('--archive-approval', type=Path)
    parser.add_argument('--archive-approval-sha256')
    parser.add_argument('--continuation-receipt-sha256')
    args = parser.parse_args()
    continuation_values = (args.continuation_contract, args.continuation_contract_sha256,
                           args.archive_approval, args.archive_approval_sha256,
                           args.continuation_receipt_sha256)
    continuation_requested = any(value is not None for value in continuation_values)
    if continuation_requested and (args.phase != 'consumer' or not all(continuation_values)):
        parser.error('Continuation requires consumer phase and all five exact continuation arguments')
    if args.phase == 'consumer' and (not args.producer_dir or not args.durable_backup_receipt):
        parser.error('Consumer requires --producer-dir and --durable-backup-receipt')
    if args.phase == 'producer' and (args.producer_dir or args.durable_backup_receipt):
        parser.error('Producer does not accept consumer resume inputs')
    if not 120 <= args.seconds <= 900:
        parser.error('--seconds must be in 120..900')
    root, folder = args.root.resolve(), args.out.resolve()
    if folder.exists() or folder == root or root in folder.parents:
        parser.error('--out must be a new evidence directory outside the repository')
    consumer_path = args.consumer_build.resolve()
    consumer_bytes = consumer_path.read_bytes()
    consumer = json.loads(consumer_bytes)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()

    def clean_source():
        return (subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() == head and
                subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True) == '')

    demand(clean_source(), 'Require clean committed parent build source')
    demand(isinstance(consumer, dict) and consumer.get('status') == 'PASS_XBOX_BUILD' and
           consumer.get('source_commit') == head, 'Manifest must identify the current successfully built source')
    for key in ('xbe', 'map'):
        value = consumer.get(key, '')
        demand(isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value),
               'Invalid consumer manifest hash: ' + key)
    sys.path.insert(0, str(root / 'tools'))
    continuation = continuation_module = preadmitted = None
    if continuation_requested:
        import xemu_natural_save_continuation as continuation_module
        continuation = continuation_module.load_context(root,
            args.continuation_contract, args.continuation_contract_sha256,
            args.archive_approval, args.archive_approval_sha256,
            args.durable_backup_receipt, args.continuation_receipt_sha256)
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_original_startup_admission import original_recipe, NATIVE_SYMBOLS, SYMBOLS as STARTUP_SYMBOLS
    from xemu_guest_snapshot import words
    from xemu_original_switch_sound import exact_address
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    # Only the read-only VPP reader is reused from the fixture module.
    demand(native.ROOT.resolve() == root, 'Native runner belongs to another repository')
    disc, iso = root / 'build/xbox/disc', root / 'build/xbox/redfaction-diagnostic.iso'
    xbe, mapping = disc / 'default.xbe', root / 'build/xbox/main.map'
    base = root / 'local/xemu-harness/pacing-base.qcow2'
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (xbe, mapping, base, packer):
        demand(path.is_file(), 'Missing existing parent-built/owned input: ' + str(path))
    demand(sha256(xbe) == consumer['xbe'] and sha256(mapping) == consumer['map'],
           'Existing XBE/map do not match the successful parent build manifest')
    standalone(base)
    map_text = mapping.read_text()
    # Retain the established startup terminal diagnostics without mutating
    # either source mapping or changing inputs, admission or live sampling.
    terminal_symbols = MappingProxyType({**STARTUP_SYMBOLS, **SYMBOLS})
    addresses = {name: exact_address(map_text, name) for name in
                 NATIVE_SYMBOLS + tuple(terminal_symbols) + tuple(LIVE_SYMBOLS) + ('rf_scene_world_checkpoint_data',)}
    recipe = original_recipe(root, read_entry, inspect_level, 'L3S1.rfl')
    recipe.update(frames=SAVE_FRAMES, input='RFI6 neutral180; separate normal save API schedule at120',
        producer_input='Neutral180, normal save API at120',
        consumer_frames=LOAD_FRAMES, consumer_input='Neutral except forward120..134 and primary180',
        limits=LIMITS, historical_regression_recovered=False)
    report = dict(status='NOT_RUN', recipe=recipe, attempts=0, limits=recipe['limits'],
        source_commit=head, consumer_build=consumer, consumer_build_path=str(consumer_path),
        consumer_build_sha256=hashlib.sha256(consumer_bytes).hexdigest(), symbol_addresses=addresses,
        phase=args.phase, hdd=str(base), hdd_mode='NEW private producer persistent, immutable after strict clean exit; cloned snapshot consumer',
        producer_attempts=0, consumer_attempts=0, producer_promoted=False, fresh_load_verified=False)
    require_no_project_xemu(root)
    lock = SessionLock(root)
    lock.acquire()
    old_lock, old_address, originals = native.SessionLock, native.address, None
    staged = iso_moved = folder_created = False
    protected_disks, snapshot_disks = [], []
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        folder_created = True
        (folder / 'consumer-build.json').write_bytes(consumer_bytes)
        report['base_hdd_sha256'] = sha256(base)
        demand(report['base_hdd_sha256'] == BASE_HDD_SHA256, 'Restored baseline HDD differs from verified B5DF input')
        report['original_disc_names'] = sorted(path.name for path in disc.iterdir())
        archives = {}
        for path in sorted(disc.iterdir()):
            if not path.is_file() or (path.suffix != '.vpp' and path.name != 'bluebeard.bty') or path.name == 'scene-fixture.vpp':
                continue
            source = root / 'Installed_Game' / path.name
            demand(source.is_file(), 'Non-original disc archive: ' + path.name)
            archives[path.name] = sha256(source)
            demand(sha256(path) == archives[path.name], 'Disc archive differs from original: ' + path.name)
        demand({recipe['archive'], 'tables.vpp', 'meshes.vpp', 'motions.vpp', 'audio.vpp', 'bluebeard.bty'} <= archives.keys(),
               'Required original assets missing')
        if recipe['level'] == 'L3S1.rfl':
            demand(archives['levels1.vpp'] == '7462fc400a347232139c66c203c4a1c9379d7bfc400f93e1ab614ff67755aec1',
                   'Require hash-pinned original levels1.vpp for L3S1')
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
        if continuation is not None:
            # Explicit continuation admission precedes the first media mutation.
            # The original caller still uses admit_producer() with current HEAD.
            original_producer_build = continuation_module.precheck_producer(continuation, args.producer_dir)
            preadmitted = read_producer_evidence(args.producer_dir,
                continuation['contract']['producer_source_commit'], original_producer_build, sha256)
            original_checks = producer_checks(preadmitted['producer'],
                preadmitted['verification']['recipe'], preadmitted['saved'],
                preadmitted['manifest']['shutdown'])
            demand(original_checks == preadmitted['verification']['producer_checks'] and
                   all(type(value) is bool and value for value in original_checks.values()),
                   'Original producer checks no longer recompute exactly')
            preadmitted = continuation_module.admit(continuation, preadmitted, report,
                                                    consumer_path, folder)
            report['continuation'] = continuation_module.report_identity(continuation)
            report['hdd_mode'] = 'Verified remote reconstruction; independent snapshot consumer copy'
            (folder / 'continuation-contract.json').write_bytes(continuation['contract_bytes'])
        if iso.exists():
            iso.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in originals:
            (disc / name).unlink(missing_ok=True)
        for name in ('campaign-spawn.flag', 'scene-preview.flag', 'player-control.flag'):
            (disc / name).write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(
            recipe['archive'].encode('ascii').ljust(64, b'\0') + recipe['level'].encode('ascii').ljust(64, b'\0'))
        def pack_phase(name, consumer_phase):
            if continuation is not None:
                demand(continuation_module.protected_unchanged(continuation),
                       'Qualified raw build/admission evidence changed before pack')
            expected = recording(consumer_phase)
            (disc / 'player-replay.bin').write_bytes(expected)
            for control in ('campaign-quick-actions.bin', 'world-hdd-load.flag', 'world-hdd-save.flag'):
                (disc / control).unlink(missing_ok=True)
            if consumer_phase:
                (disc / 'world-hdd-load.flag').write_bytes(b'')
            else:
                (disc / 'campaign-quick-actions.bin').write_bytes(struct.pack('<2I', SAVE_AT, UINT_MAX))
            expected_controls = {
                'campaign-spawn.flag': b'', 'scene-preview.flag': b'', 'player-control.flag': b'',
                'campaign-level.bin': recipe['archive'].encode('ascii').ljust(64, b'\0') +
                                      recipe['level'].encode('ascii').ljust(64, b'\0'),
                'player-replay.bin': expected,
            }
            if consumer_phase:
                expected_controls['world-hdd-load.flag'] = b''
            else:
                expected_controls['campaign-quick-actions.bin'] = struct.pack('<2I', SAVE_AT, UINT_MAX)
            actual_controls = {path.name for path in disc.iterdir() if path.is_file() and
                (path.name == 'scene-fixture.vpp' or path.name.startswith('campaign-') or
                 (path.suffix in ('.flag', '.bin', '.txt') and path.name not in immutable))}
            demand(actual_controls == set(expected_controls), 'Unexpected staged selector/input: ' + name)
            demand(all((disc / key).read_bytes() == value for key,value in expected_controls.items()),
                   'Staged input bytes changed: ' + name)
            report[name + '_selector_allowlist_verified'] = True
            # Save inputs as evidence; never synthesize a serialized checkpoint.
            (folder / (name + '-input.bin')).write_bytes(expected)
            report[name + '_input_sha256'] = hashlib.sha256(expected).hexdigest()
            iso.unlink(missing_ok=True)
            with (folder / (name + '-pack-only.log')).open('wb') as log:
                subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
            demand(sha256(xbe) == consumer['xbe'] and sha256(mapping) == consumer['map'] and clean_source(),
                   'Pinned build/source changed before ' + name)
            report[name + '_iso_sha256'] = sha256(iso)
            report[name + '_disc_flags'] = {name: sha256(disc / name) for name in sorted(originals)
                                           if (disc / name).is_file()}
            write_json(folder / 'verification.json', report)

        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)
        native.address = exact_address
        if args.phase == 'producer':
            # New provenance and path. Never use the missing historical save.
            producer_disk = folder / 'new-l3s1-producer.qcow2'
            backup_disk = folder / 'new-l3s1-immutable-backup.qcow2'
            demand(shutil.disk_usage(folder).free >= base.stat().st_size*2 + 2*1024**3,
                   'Need producer, immutable local backup and 2GiB reserve')
            shutil.copyfile(base, producer_disk)
            standalone(producer_disk)
            demand(sha256(producer_disk) == BASE_HDD_SHA256, 'Producer base copy differs')
            write_json(folder / 'owner.json', dict(purpose='NEW_L3S1_NATURAL_SAVE',
                base=str(base.resolve()), base_sha256=BASE_HDD_SHA256,
                producer=str(producer_disk), backup=str(backup_disk),
                missing_historical_hdd_sha256='129855eb9c0f311c52656c35f5443913a2a18b93e5ee1d348c5470e77139373b'))
            pack_phase('producer', False)
            report['attempts'] = report['producer_attempts'] = 1
            with (folder / 'producer-attempt.json').open('x') as marker:
                marker.write(json.dumps(dict(source_commit=head, xbe=consumer['xbe'],
                                            map=consumer['map'], frames=SAVE_FRAMES, save_frame=SAVE_AT)))
            write_json(folder / 'verification.json', report)
            guest = native.run_guest(folder, 'producer', producer_disk, SAVE_FRAMES, args.seconds,
                snapshot=False, extra_symbols=terminal_symbols, allow_guest_error=True,
                capture_world=True, measure_fps=False, require_clean_shutdown=True,
                probe=live_reader(folder, 'producer-live-before-save', 30, SAVE_AT,
                                  words, exact_address, write_json), probe_frame=30)
            report['producer'] = guest
            shutdown = json.loads((folder / 'producer/shutdown.json').read_text())
            report['producer_shutdown'] = shutdown
            saved = decode_actual_payload(folder / 'producer/xbox-world.rfwc')
            report['saved_payload'] = saved
            report['producer_checks'] = producer_checks(guest, recipe, saved, shutdown)
            write_json(folder / 'verification.json', report)
            demand(all(report['producer_checks'].values()), 'Producer failed native live-save/flush/shutdown checks')
            require_no_project_xemu(root)
            digest = sha256(producer_disk)
            report['producer_hdd_sha256'] = digest
            report['producer_hdd_bytes'] = producer_disk.stat().st_size
            producer_disk.chmod(0o444)
            demand(not producer_disk.stat().st_mode & 0o222, 'Producer is not read-only')
            protected_disks.append((producer_disk, digest))
            shutil.copyfile(producer_disk, backup_disk)
            standalone(backup_disk)
            demand(sha256(backup_disk) == digest, 'Immutable backup copy differs')
            backup_disk.chmod(0o444)
            demand(not backup_disk.stat().st_mode & 0o222, 'Backup is not read-only')
            protected_disks.append((backup_disk, digest))
            report['immutable_backup'] = dict(path=str(backup_disk), sha256=digest,
                                            bytes=backup_disk.stat().st_size, read_only=True,
                                            durable_off_workspace=False)
            report['producer_promoted'] = True
            report['status'] = 'PASS_NEW_NATIVE_SAVE_AWAITING_DURABLE_BACKUP'
            write_json(folder / 'producer-immutable.json', dict(
                purpose='NEW_L3S1_NATURAL_SAVE', path=str(producer_disk), backup=str(backup_disk), read_only=True,
                sha256=digest, bytes=report['producer_hdd_bytes'], source_commit=head,
                xbe=consumer['xbe'], map=consumer['map'], payload_sha256=saved['sha256'],
                generation=guest['checkpoint_state'][5], slot=guest['checkpoint_state'][6],
                shutdown=shutdown, historical_regression_recovered=False))
            # Deliberately stop here. Parent must preserve exact disk remotely,
            # verify its upload, and provide the receipt to a separate consumer.
            report['consumer_blocker'] = 'PARENT_VERIFIED_DURABLE_BACKUP_REQUIRED'
            write_json(folder / 'verification.json', report)
        else:
            admitted = preadmitted if continuation is not None else admit_producer(
                args.producer_dir, args.durable_backup_receipt, head, consumer, sha256)
            demand(admitted['verification'].get('original_archive_sha256') == report['original_archive_sha256'],
                   'Consumer original assets differ from exact producer assets')
            producer_disk, backup_disk = admitted['disk'], admitted['backup']
            saved, guest = admitted['saved'], admitted['producer']
            digest = admitted['manifest']['sha256']
            protected_disks.extend(((producer_disk, digest), (backup_disk, digest)))
            report['producer_evidence'] = dict(folder=str(admitted['folder']),
                verification_sha256=admitted['verification_sha256'],
                immutable_manifest_sha256=admitted['manifest_sha256'],
                producer_hdd_sha256=digest, producer_hdd_bytes=admitted['manifest']['bytes'])
            report['durable_backup_receipt'] = admitted['receipt']
            report['durable_backup_receipt_sha256'] = admitted['receipt_sha256']
            if continuation is not None:
                (folder / 'parent-durable-backup-receipt.json').write_bytes(admitted['receipt_bytes'])
                (folder / 'producer-verification.json').write_bytes(admitted['verification_bytes'])
                (folder / 'producer-immutable.json').write_bytes(admitted['manifest_bytes'])
            else:
                (folder / 'parent-durable-backup-receipt.json').write_bytes(args.durable_backup_receipt.read_bytes())
                (folder / 'producer-verification.json').write_bytes((admitted['folder'] / 'verification.json').read_bytes())
                (folder / 'producer-immutable.json').write_bytes((admitted['folder'] / 'producer-immutable.json').read_bytes())
            consumer_disk = folder / 'new-l3s1-consumer.qcow2'
            demand(shutil.disk_usage(folder).free >= producer_disk.stat().st_size + 2*1024**3,
                   'Need isolated consumer copy and 2GiB reserve')
            consumer_seed = admitted['consumer_seed'] if continuation is not None else producer_disk
            shutil.copyfile(consumer_seed, consumer_disk)
            standalone(consumer_disk)
            demand(sha256(consumer_disk) == digest, 'Consumer copy differs from exact new producer')
            snapshot_disks.append((consumer_disk, digest))
            pack_phase('consumer', True)
            require_no_project_xemu(root)
            attempt = (continuation_module.before_launch(continuation, consumer_disk)
                if continuation is not None else dict(out=str(folder), source_commit=head,
                    producer_sha256=digest, backup_receipt_sha256=admitted['receipt_sha256']))
            # Atomically reserve this producer's sole consumer attempt, even
            # across distinct consumer output folders. Never remove on failure.
            with (admitted['folder'] / 'consumer-attempt.json').open('x') as marker:
                marker.write(json.dumps(attempt))
                if continuation is not None:
                    marker.flush()
                    os.fsync(marker.fileno())
            if continuation is not None:
                directory = os.open(admitted['folder'], os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(directory)
                finally:
                    os.close(directory)
            report['attempts'] = report['consumer_attempts'] = 1
            write_json(folder / 'verification.json', report)
            loaded = native.run_guest(folder, 'consumer', consumer_disk, LOAD_FRAMES, args.seconds,
                snapshot=True, extra_symbols=terminal_symbols, allow_guest_error=True,
                capture_world=False, measure_fps=False, require_clean_shutdown=True,
                probe=live_reader(folder, 'consumer-live-restored', 30, MOVE_BEGIN,
                                  words, exact_address, write_json), probe_frame=30,
                final_probe=live_reader(folder, 'consumer-live-after-move', 140, FIRE_AT,
                                        words, exact_address, write_json), final_probe_frame=140)
            report['consumer'] = loaded
            report['consumer_shutdown'] = json.loads((folder / 'consumer/shutdown.json').read_text())
            checks, observed = consumer_checks(loaded, recipe, saved, guest, report['consumer_shutdown'])
            report['consumer_checks'], report['gameplay_observation'] = checks, observed
            gameplay = {'movement_observed', 'one_actual_shot', 'one_round_consumed', 'no_intervening_pickup'}
            report['fresh_load_verified'] = all(value for key,value in checks.items() if key not in gameplay)
            report['movement_verified'] = checks['movement_observed']
            report['fire_verified'] = checks['one_actual_shot'] and checks['one_round_consumed'] and checks['no_intervening_pickup']
            report['status'] = 'PASS_NEW_NATIVE_SAVE_LOAD_AND_BOUNDED_CONTROLS' if all(checks.values()) else 'CHECK_FAILED'
            write_json(folder / 'verification.json', report)
    except BaseException as error:
        report.update(status='CHECK_FAILED', error=type(error).__name__ + ': ' + str(error))
        raise
    finally:
        native.SessionLock, native.address = old_lock, old_address
        errors = []
        try:
            if staged:
                for name, data in originals.items():
                    try:
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
                for name, path in (('xbe', xbe), ('map', mapping)):
                    backup = folder / ('tested-default.xbe' if name == 'xbe' else 'tested-main.map')
                    try:
                        if not path.is_file() or sha256(path) != pins[name]:
                            errors.append(name + ': unexpected change; restoring preserved bytes')
                            if backup.is_file() and sha256(backup) == pins[name]:
                                temporary = path.with_name(path.name + '.startup-admission-restore')
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
            verify('manifest_source_remains_clean', clean_source)
            if continuation is not None:
                verify('continuation_protected_inputs_unchanged',
                       lambda: continuation_module.protected_unchanged(continuation))
            for path, digest in protected_disks:
                verify('protected_unchanged_' + path.name, lambda path=path, digest=digest:
                       sha256(path) == digest and not path.stat().st_mode & 0o222)
            for path, digest in snapshot_disks:
                verify('snapshot_unchanged_' + path.name, lambda path=path, digest=digest: sha256(path) == digest)
            report['native_result_paths'] = [str(path.relative_to(folder)) for path in
                (folder / 'producer/result.json', folder / 'consumer/result.json') if path.is_file()]
            report['shutdown_receipt_paths'] = [str(path.relative_to(folder)) for path in
                (folder / 'producer/shutdown.json', folder / 'consumer/shutdown.json') if path.is_file()]
            report['restoration_errors'] = errors
            if errors:
                report['status'] = 'CHECK_FAILED'
            if folder_created:
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    expected_status = ('PASS_NEW_NATIVE_SAVE_AWAITING_DURABLE_BACKUP' if args.phase == 'producer'
                       else 'PASS_NEW_NATIVE_SAVE_LOAD_AND_BOUNDED_CONTROLS')
    if report['status'] != expected_status:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
