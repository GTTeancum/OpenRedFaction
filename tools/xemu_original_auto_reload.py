"""Parent-only original L3S1 automatic reload after one hourly Xbox build.

One 780-frame stock 64 MiB snapshot run with sixteen ordinary primary presses.
Movement, look and every other input stay neutral; release after the last shot.
Pack the manifest-pinned XBE without make; never create a gameplay fixture, alter original assets,
grant inventory, inject events, save/load, capture images/audio, measure FPS,
send host input, adjust aim, search a route, or retry. This verifies functional
autonomous reload, not exact animation timing or authored mission inventory.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
os.environ.setdefault('RF_XEMU_ROOT', '/workspace/shared/xemu-inputs')
os.environ.setdefault('RF_XEMU_BINARY', '/workspace/shared/xemu/squashfs-root/usr/bin/xemu')

FRAMES, EARLY_FRAME, FIRE_BEGIN, FIRE_END = 780, 30, 120, 570
FIRE_FRAMES = tuple(range(FIRE_BEGIN, FIRE_END + 1, 30))
SYMBOLS = {
    'rf_scene_player_spawn_diagnostic': 19, 'rf_scene_weapon_supply': 4,
    'rf_scene_player_ammo': 8, 'rf_scene_combat': 8,
    'rf_scene_pistol_rules': 7, 'rf_scene_weapon_selection': 8,
    'rf_scene_setup_result': 4, 'rf_scene_script_grants': 8,
    'rf_scene_section_autosave': 4, 'rf_scene_pickups': 8,
    'rf_scene_weapon_impact_audio': 16,  # Passive admission/owner telemetry only; no audio acceptance.
}
OWNER_SYMBOLS = ('campaign_pistol_id', 'campaign_player_inventory',
                 'campaign_equipped_slot', 'campaign_explicit_unarmed')
NATIVE_SYMBOLS = (
    'rf_diagnostic', 'rf_scene_world_checkpoint_state',
    'rf_xbox_checkpoint_storage_state', 'rf_scene_player_life',
    'rf_scene_checkpoint_world_route_contact', 'rf_xbox_level_transitions',
    'rf_player_replay_diagnostic', 'rf_scene_actor_frame_count',
    'rf_scene_campaign_load_stage', 'rf_scene_follow_level_exits',
    'rf_scene_level_transition',
)
LIMITS = ('One original L3S1 stationary sixteen-shot magazine and autonomous refill '
          'using the unchanged port startup supply. No authored mission-inventory, '
          'animation-timing parity, aiming, route, save/load, audio-output, visual or FPS claim.')


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def original_recipe(root, read_entry, inspect_level):
    data = read_entry(root / 'Installed_Game/levels1.vpp', 'L3S1.rfl')
    meta = inspect_level(io.BytesIO(data), dict(offset=0, size=len(data), name='L3S1.rfl'))
    demand(meta['version'] == 180, 'Require original version 180 L3S1')
    starts = [row for row in meta['sections'] if row['type'] == '0x70000']
    demand(len(starts) == 1 and starts[0]['size'] == 48, 'Missing/ambiguous original player start')
    start = list(struct.unpack_from('<12I', data, starts[0]['offset'] + 8))
    # level.c converts disk forward/right/up into runtime right/up/forward.
    return dict(level='L3S1.rfl', archive='levels1.vpp',
        rfl_sha256=hashlib.sha256(data).hexdigest(), rfl_bytes=len(data),
        spawn_words=[1] + start[:3] + start[6:12] + start[3:6], frames=FRAMES,
        input='RFI6: 780 records; primary is one only at 120,150,...,570. Every other field is zero.',
        fire_frames=list(FIRE_FRAMES), released_frames=[FIRE_END + 1, FRAMES - 1],
        early_observation=[EARLY_FRAME, FIRE_BEGIN - 1],
        inventory_policy='Existing port first-pass pistol supply, not authored original-mission inventory.',
        limits=LIMITS)


def ordinary_recording():
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 0, int(frame in FIRE_FRAMES), 0, 0, 0)
        for frame in range(FRAMES))


def early_reader(folder, recipe, words, exact_address, write_json):
    def read(monitor, mapping):
        get = lambda name, count: words(monitor, exact_address(mapping, name), count)
        diagnostic = get('rf_diagnostic', 58)
        row = dict(frame=diagnostic[37], phase=diagnostic[2], raw={})
        write_json(folder / 'initial-owner.json', row)
        demand(row['phase'] == 2 and EARLY_FRAME <= row['frame'] < FIRE_BEGIN,
               'Missed required live pre-fire inventory window; no alternate attempt')
        for name, count in SYMBOLS.items():
            row['raw'][name] = get(name, count)
        # include/rf/weapon.h: Xbox32 owner is 64 owned bytes, 32 reserve
        # int32s, then 64 loaded int32s (448 bytes). Read it only while live;
        # terminal scene cleanup may clear private inventory/selection owners.
        inventory_words = get('campaign_player_inventory', 112)
        inventory_bytes = struct.pack('<112I', *inventory_words)
        row['inventory_words'] = inventory_words
        row['player'] = dict(pistol_id=get('campaign_pistol_id', 1)[0],
            equipped_slot=get('campaign_equipped_slot', 1)[0],
            explicit_unarmed=get('campaign_explicit_unarmed', 1)[0],
            pistol_owned=bool(inventory_bytes[3]),
            pistol_loaded=struct.unpack_from('<i', inventory_bytes, 64 + 32*4 + 3*4)[0])
        row['checks'] = initial_checks(row, recipe)
        write_json(folder / 'initial-owner.json', row)
        demand(all(row['checks'].values()), 'Initial owned/selected pistol or ordinary-start admission failed')
        return row
    return read


def initial_checks(row, recipe):
    raw, player = row.get('raw', {}), row.get('player', {})
    ammo, combat = raw.get('rf_scene_player_ammo', []), raw.get('rf_scene_combat', [])
    return dict(
        live_before_fire=row.get('phase') == 2 and EARLY_FRAME <= row.get('frame', FRAMES) < FIRE_BEGIN,
        actual_selected_owned_pistol=player.get('pistol_id') == 3 and player.get('equipped_slot') == 0 and
            player.get('explicit_unarmed') == 0 and player.get('pistol_owned') is True and player.get('pistol_loaded') == 16,
        initial_ammo=ammo == [3, 125, 16, 0, 0, 0, 448, 0],
        initially_unfired=len(combat) == 8 and combat[0] == 0 and combat[5:] == [16, 0, 0],
        original_spawn_and_basis=raw.get('rf_scene_player_spawn_diagnostic', [])[:13] == recipe['spawn_words'],
        no_setup=raw.get('rf_scene_setup_result') == [0]*4,
        no_script_grant=raw.get('rf_scene_script_grants') == [0]*8,
    )


def evaluate(guest, recipe):
    extra = guest.get('extra', {})
    supply = extra.get('rf_scene_weapon_supply', [])
    ammo, combat = extra.get('rf_scene_player_ammo', []), extra.get('rf_scene_combat', [])
    rules, selection = extra.get('rf_scene_pistol_rules', []), extra.get('rf_scene_weapon_selection', [])
    pickups = extra.get('rf_scene_pickups', [])
    initial = initial_checks(guest.get('probe', {}), recipe)
    checks = dict(
        stock64=guest.get('memory_bytes') == 64 * 1024 * 1024,
        terminal_complete=guest.get('guest_phase') == 5 and guest.get('frames') == FRAMES,
        exact_replay_completed=guest.get('replay_state', [])[1:] == [FRAMES, FRAMES, 0] and
                              guest.get('actor_frame_count') == FRAMES,
        initial_owned_inventory=all(initial.values()),
        authored_primary_rules=len(rules) == 7 and rules[:3] == [16, 66, 30] and rules[4] == 1,
        sixteen_actual_shots=len(combat) == 8 and combat[0] == 16 and combat[5:] == [16, 0, 0],
        one_exact_reload=ammo == [3, 109, 16, 16, 1, 0, 448, 0],
        ammo_conserved=len(ammo) == 8 and len(combat) == 8 and ammo[1] + ammo[2] + combat[0] == 141,
        unchanged_selection=len(selection) == 8 and selection[:2] == [0, 0],
        no_setup=extra.get('rf_scene_setup_result') == [0]*4,
        no_script_grant=extra.get('rf_scene_script_grants') == [0]*8,
        no_placed_pickup=len(pickups) == 8 and pickups[3:5] == [0, 0] and pickups[7] == 0,
        no_save_or_load=guest.get('checkpoint_state') == [0]*10 and guest.get('storage_state') == [0]*8 and
                        extra.get('rf_scene_section_autosave') == [0]*4,
        no_death_or_respawn=guest.get('player_life', [])[:3] == [0, 0, 0],
        no_transition=guest.get('level_transitions', [None])[0] == 0 and
                      guest.get('level_request', [None])[0] == 0,
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        original_spawn_and_basis=extra.get('rf_scene_player_spawn_diagnostic', [])[:13] == recipe['spawn_words'],
        weapon_tables_admitted=len(supply) == 4 and 0 < supply[0] <= 64 and
                               0 < supply[1] <= supply[0] and supply[2] > 0,
    )
    # Exact sixteen-shot consumption and one sixteen-round transfer after a
    # replay with no reload or seventeenth/empty trigger proves the functional
    # automatic reload. Do not demand a short intermediate animation sample.
    failed = [name for name, passed in checks.items() if not passed]
    return dict(status='CHECK_FAILED' if failed else 'PASS_ORIGINAL_AUTOMATIC_RELOAD',
                checks=checks, initial_checks=initial, failed_checks=failed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--consumer-build', type=Path, required=True,
                        help='PASS_XBOX_BUILD manifest with exact source_commit, xbe and map hashes')
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
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
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_original_switch_sound import exact_address
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    from xemu_guest_snapshot import words
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
    addresses = {name: exact_address(map_text, name) for name in NATIVE_SYMBOLS + tuple(SYMBOLS) + OWNER_SYMBOLS}
    recipe = original_recipe(root, read_entry, inspect_level)
    report = dict(status='NOT_RUN', recipe=recipe, attempts=0, limits=LIMITS,
        source_commit=head, consumer_build=consumer, consumer_build_path=str(consumer_path),
        consumer_build_sha256=hashlib.sha256(consumer_bytes).hexdigest(), symbol_addresses=addresses,
        hdd=str(base), hdd_mode='Owned standalone base, XEMU -snapshot; no save/load flags or HDD copy')
    require_no_project_xemu(root)
    lock = SessionLock(root)
    lock.acquire()
    old_lock, old_address, originals = native.SessionLock, native.address, None
    staged = iso_moved = folder_created = False
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        folder_created = True
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
        demand({'levels1.vpp', 'tables.vpp', 'meshes.vpp', 'motions.vpp', 'audio.vpp', 'bluebeard.bty'} <= archives.keys(),
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
        for name in ('campaign-spawn.flag', 'scene-preview.flag', 'player-control.flag'):
            (disc / name).write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L3S1.rfl'.ljust(64, b'\0'))
        recording = ordinary_recording()
        (disc / 'player-replay.bin').write_bytes(recording)
        (folder / 'ordinary-input.bin').write_bytes(recording)
        allowed = {'campaign-spawn.flag', 'scene-preview.flag', 'player-control.flag',
                   'campaign-level.bin', 'player-replay.bin'}
        demand(all((disc / name).exists() == (name in allowed) for name in originals),
               'Unrequested setup/save/fixture selector survived staging')
        demand((disc / 'player-replay.bin').read_bytes() == ordinary_recording(), 'Ordinary input changed during staging')
        report['staged_selectors'] = sorted(allowed)
        report['replay_sha256'] = hashlib.sha256(recording).hexdigest()
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        demand(sha256(xbe) == consumer['xbe'] and sha256(mapping) == consumer['map'] and clean_source(),
               'Clean manifest-pinned build source or XBE/map changed before launch')
        report['tested_iso_sha256'] = sha256(iso)
        # Scoped overrides affect only the shared runner's lock and read-only
        # symbol lookup. Every shared/base/extra lookup requires an exact token.
        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)
        native.address = exact_address
        report['attempts'] = 1
        write_json(folder / 'verification.json', report)
        guest = native.run_guest(folder, 'native', base, FRAMES, args.seconds, snapshot=True,
            extra_symbols=SYMBOLS, allow_guest_error=True, capture_world=False, measure_fps=False,
            probe=early_reader(folder, recipe, words, exact_address, write_json), probe_frame=EARLY_FRAME)
        report['native'] = guest
        # Preserve the raw result before any acceptance predicate; the shared
        # runner also writes native/result.json before raising terminal errors.
        write_json(folder / 'native-result.json', guest)
        report.update(evaluate(guest, recipe))
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
                                temporary = path.with_name(path.name + '.automatic-reload-restore')
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
            report['native_result_path'] = ('native/result.json' if folder_created and
                                           (folder / 'native/result.json').is_file() else None)
            report['restoration_errors'] = errors
            if errors:
                report['status'] = 'CHECK_FAILED'
            if folder_created:
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != 'PASS_ORIGINAL_AUTOMATIC_RELOAD':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
