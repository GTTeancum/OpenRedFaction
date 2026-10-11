"""Parent-only original L3S1 fresh-start bounded controls, never save/load.

One stock64 snapshot run: forward120..134 and primary180, otherwise neutral.
No build, fixture, grant, forced event, guest-state write, host input, image,
audio capture, FPS measurement, campaign route or checkpoint-readiness test.
This is independent of the genuine-save producer and gated saved-load consumer.
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
FRAMES, MOVE_BEGIN, MOVE_END, FIRE_AT = 240, 120, 135, 180
PURPOSE = 'ORIGINAL_L3S1_FRESH_CONTROLS'
PASS = 'PASS_ORIGINAL_FRESH_START_BOUNDED_CONTROLS'
LIMITS = ('Independent original L3S1 fresh start; horizontal displacement between '
          'live samples surrounding fifteen forward-input frames and one actual '
          'primary shot/round consumption. Actual fresh constructor inventory, '
          'not authored mission-inventory proof. No save/load, persistence, '
          'historical L11S3 recovery, promised contact, hit/damage validation, '
          'campaign route, physical controller, audio, image or FPS claim.')


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def recording():
    return b'RFI6' + struct.pack('<I', 48) + b''.join(
        struct.pack('<5f7I', 0, 0, int(MOVE_BEGIN <= frame < MOVE_END),
                    0, 0, 0, 0, 0, int(frame == FIRE_AT), 0, 0, 0)
        for frame in range(FRAMES))


def fresh_checks(guest, shutdown):
    """Actual fresh live owners only. No saved payload or restore predicate."""
    extra = guest.get('extra', {})
    pre, moved = guest.get('probe', {}), guest.get('final_probe', {})
    a, b = pre.get('raw', {}), moved.get('raw', {})
    supply = extra.get('rf_scene_weapon_supply', [])
    selected = dict(pistol_id=3, slot=0, explicit_unarmed=0,
                    pistol_owned=1, pistol_loaded=16)
    checkpoints = (a.get('rf_scene_world_checkpoint_state'),
                   b.get('rf_scene_world_checkpoint_state'), guest.get('checkpoint_state'))
    storage = (a.get('rf_xbox_checkpoint_storage_state'),
               b.get('rf_xbox_checkpoint_storage_state'), guest.get('storage_state'))
    # Source-qualified for this exact no-selector fresh flow: scene.c resets
    # storage at scene_checkpoint_begin; every world/geomod HDD path is opt-in.
    # Named world fields8/9 are attempted flags and1/2/4 are completion bytes.
    # Storage is last-operation metadata, not a cumulative save/load counter.
    checks = dict(
        weapon_tables_admitted=len(supply) == 4 and 0 < supply[0] <= 64 and
                               0 < supply[1] <= supply[0] and supply[2] > 0,
        no_world_save_or_load=all(state == [0]*10 for state in checkpoints),
        no_storage_activity=all(state == [0]*8 for state in storage),
        actual_fresh_pistol=pre.get('selected') == moved.get('selected') == selected,
        prefire_inventory_unchanged=bool(pre.get('inventory_hex')) and
                                   pre.get('inventory_hex') == moved.get('inventory_hex'),
        strict_shutdown=shutdown.get('clean') is True and
                        shutdown.get('qmp_quit_acknowledged') is True and
                        shutdown.get('normal_wait_completed') is True and
                        shutdown.get('returncode') == 0 and
                        shutdown.get('forced_cleanup_used') is False)
    positions = pre.get('position', []) + moved.get('position', [])
    valid = len(positions) == 6 and all(v is not None and math.isfinite(v) for v in positions)
    displacement = sum((x-y)**2 for x,y in zip(pre.get('position', []), moved.get('position', []))) if valid else None
    horizontal = sum((pre['position'][i]-moved['position'][i])**2 for i in (0,2)) if valid else None
    rings = b.get('rf_scene_actor_ring_frames', [])
    inputs = b.get('rf_scene_actor_input_frames', [])
    tagged_move = all(len(rings) == 64 and rings[frame%64] == frame and len(inputs) == 192 and
        struct.unpack('<3f', struct.pack('<3I', *inputs[(frame%64)*3:(frame%64)*3+3])) == (0.0,0.0,1.0)
        for frame in range(MOVE_BEGIN, MOVE_END))
    end_ammo, end_combat = extra['rf_scene_player_ammo'], extra['rf_scene_combat']
    checks['movement_observed'] = valid and horizontal > .0001 and tagged_move
    checks['one_actual_shot'] = bool(a and b) and a['rf_scene_combat'][0] == 0 and \
        b['rf_scene_combat'][0] == 0 and end_combat[0] == 1
    checks['one_round_consumed'] = bool(a and b) and \
        a['rf_scene_player_ammo'] == b['rf_scene_player_ammo'] == [3,125,16,0,0,0,448,0] and \
        end_ammo == [3,125,15,0,0,0,448,0]
    checks['no_intervening_pickup'] = bool(a and b) and \
        a['rf_scene_pickups'][3:5] == b['rf_scene_pickups'][3:5] == extra['rf_scene_pickups'][3:5] == [0,0] and \
        a['rf_scene_pickups'][7] == b['rf_scene_pickups'][7] == extra['rf_scene_pickups'][7] == 0
    observed = dict(before=pre.get('position'), after=moved.get('position'),
        before_frame=pre.get('frame'), after_frame=moved.get('frame'),
        displacement_squared=displacement, horizontal_displacement_squared=horizontal,
        tagged_movement_input=tagged_move, actual_shots=end_combat[0],
        actual_hits=end_combat[1], actual_kills=end_combat[2],
        movement_scope='Horizontal displacement between live samples surrounding the bounded forward-input segment',
        hit_damage_validated=False)
    return checks, observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--consumer-build', type=Path, required=True,
                        help='Existing PASS_XBOX_BUILD manifest: exact source_commit, xbe and map')
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if not 120 <= args.seconds <= 900:
        parser.error('--seconds must be in 120..900')
    root, folder = args.root.resolve(), args.out.resolve()
    if folder.exists() or folder == root or root in folder.parents:
        parser.error('--out must be a new evidence directory outside the repository')
    manifest_path = args.consumer_build.resolve()
    manifest_bytes = manifest_path.read_bytes()
    build = json.loads(manifest_bytes)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()

    def clean_source():
        return (subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() == head and
                subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True) == '')

    demand(clean_source(), 'Require clean committed parent build source')
    demand(isinstance(build, dict) and build.get('status') == 'PASS_XBOX_BUILD' and
           build.get('source_commit') == head, 'Manifest must identify the current successfully built source')
    for key in ('xbe', 'map'):
        value = build.get(key, '')
        demand(isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value),
               'Invalid build manifest hash: ' + key)
    sys.path.insert(0, str(root / 'tools'))
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_original_startup_admission import original_recipe, NATIVE_SYMBOLS, SYMBOLS as STARTUP_SYMBOLS
    from xemu_natural_save_gameplay import BASE_HDD_SHA256, LIVE_SYMBOLS, SYMBOLS, common_checks, live_reader
    from xemu_guest_snapshot import words
    from xemu_original_switch_sound import exact_address
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    # Only pure observations/helpers and the read-only VPP reader are reused.
    # No producer/consumer main, save payload decoder or admission is invoked.
    demand(native.ROOT.resolve() == root, 'Native runner belongs to another repository')
    disc, iso = root / 'build/xbox/disc', root / 'build/xbox/redfaction-diagnostic.iso'
    xbe, mapping = disc / 'default.xbe', root / 'build/xbox/main.map'
    base = root / 'local/xemu-harness/pacing-base.qcow2'
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (xbe, mapping, base, packer):
        demand(path.is_file(), 'Missing existing parent-built/owned input: ' + str(path))
    demand(sha256(xbe) == build['xbe'] and sha256(mapping) == build['map'],
           'Existing XBE/map do not match the successful parent build manifest')
    standalone(base)
    map_text = mapping.read_text()
    terminal_symbols = MappingProxyType({**STARTUP_SYMBOLS, **SYMBOLS})
    addresses = {name: exact_address(map_text, name) for name in
                 NATIVE_SYMBOLS + tuple(terminal_symbols) + tuple(LIVE_SYMBOLS)}
    recipe = original_recipe(root, read_entry, inspect_level, 'L3S1.rfl')
    recipe.update(frames=FRAMES, input='RFI6 neutral except forward120..134 and primary180',
                  limits=LIMITS, purpose=PURPOSE, save_load_requested=False,
                  persistence_verified=False, historical_regression_recovered=False)
    report = dict(status='NOT_RUN', purpose=PURPOSE, recipe=recipe, attempts=0, limits=LIMITS,
        source_commit=head, build_manifest=build, build_manifest_path=str(manifest_path),
        build_manifest_sha256=hashlib.sha256(manifest_bytes).hexdigest(), symbol_addresses=addresses,
        hdd=str(base), hdd_mode='Owned standalone B5DF base, XEMU -snapshot; no HDD copy or save/load request',
        save_load_requested=False, persistence_verified=False, historical_regression_recovered=False,
        fresh_start_verified=False, movement_verified=False, fire_verified=False)
    require_no_project_xemu(root)
    lock = SessionLock(root)
    lock.acquire()
    old_lock, old_address, originals = native.SessionLock, native.address, None
    staged = iso_moved = folder_created = False
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        folder_created = True
        (folder / 'build-manifest.json').write_bytes(manifest_bytes)
        report['base_hdd_sha256'] = sha256(base)
        demand(report['base_hdd_sha256'] == BASE_HDD_SHA256, 'Base HDD differs from verified B5DF input')
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
        if iso.exists():
            iso.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in originals:
            (disc / name).unlink(missing_ok=True)
        replay = recording()
        expected_controls = {
            'campaign-spawn.flag': b'', 'scene-preview.flag': b'', 'player-control.flag': b'',
            'campaign-level.bin': recipe['archive'].encode('ascii').ljust(64, b'\0') +
                                  recipe['level'].encode('ascii').ljust(64, b'\0'),
            'player-replay.bin': replay,
        }
        for name, data in expected_controls.items():
            (disc / name).write_bytes(data)
        actual_controls = {path.name for path in disc.iterdir() if path.is_file() and
            (path.name in names or path.name == 'scene-fixture.vpp' or path.name.startswith('campaign-') or
             (path.suffix in ('.flag', '.bin', '.txt') and path.name not in immutable))}
        demand(actual_controls == set(expected_controls), 'Unexpected staged fresh selector/input')
        demand(all((disc / key).read_bytes() == value for key,value in expected_controls.items()),
               'Staged fresh input bytes changed')
        report['selector_allowlist_verified'] = True
        (folder / 'fresh-controls-input.bin').write_bytes(replay)
        report['input_sha256'] = hashlib.sha256(replay).hexdigest()
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        demand(sha256(xbe) == build['xbe'] and sha256(mapping) == build['map'] and clean_source(),
               'Pinned build/source changed before fresh-controls')
        report['tested_iso_sha256'] = sha256(iso)
        report['disc_flags'] = {name: sha256(disc / name) for name in sorted(expected_controls)}
        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)
        native.address = exact_address
        require_no_project_xemu(root)
        with (folder / 'fresh-controls-attempt.json').open('x') as marker:
            marker.write(json.dumps(dict(purpose=PURPOSE, source_commit=head, xbe=build['xbe'],
                map=build['map'], frames=FRAMES, move_begin=MOVE_BEGIN, move_end=MOVE_END,
                fire_at=FIRE_AT, snapshot=True, save_load_requested=False)))
        report['attempts'] = 1
        write_json(folder / 'verification.json', report)
        guest = native.run_guest(folder, 'fresh-controls', base, FRAMES, args.seconds,
            snapshot=True, extra_symbols=terminal_symbols, allow_guest_error=True,
            capture_world=False, measure_fps=False, require_clean_shutdown=True,
            probe=live_reader(folder, 'fresh-live-before-input', 30, MOVE_BEGIN,
                              words, exact_address, write_json), probe_frame=30,
            final_probe=live_reader(folder, 'fresh-live-after-move', 140, FIRE_AT,
                                    words, exact_address, write_json), final_probe_frame=140)
        report['fresh_controls'] = guest
        shutdown = json.loads((folder / 'fresh-controls/shutdown.json').read_text())
        report['shutdown'] = shutdown
        checks = common_checks(guest, FRAMES, recipe)
        extra_checks, observed = fresh_checks(guest, shutdown)
        checks.update(extra_checks)
        report['checks'], report['gameplay_observation'] = checks, observed
        gameplay = {'movement_observed', 'one_actual_shot', 'one_round_consumed', 'no_intervening_pickup'}
        report['fresh_start_verified'] = all(value for key,value in checks.items() if key not in gameplay)
        report['movement_verified'] = checks['movement_observed']
        report['fire_verified'] = checks['one_actual_shot'] and checks['one_round_consumed'] and checks['no_intervening_pickup']
        report['status'] = PASS if all(checks.values()) else 'CHECK_FAILED'
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
                                temporary = path.with_name(path.name + '.fresh-controls-restore')
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
            report['native_result_paths'] = [str(path.relative_to(folder)) for path in
                (folder / 'fresh-controls/result.json',) if path.is_file()]
            report['shutdown_receipt_paths'] = [str(path.relative_to(folder)) for path in
                (folder / 'fresh-controls/shutdown.json',) if path.is_file()]
            report['restoration_errors'] = errors
            report['restoration_verified'] = not errors
            if errors:
                report['status'] = 'CHECK_FAILED'
                report['fresh_start_verified'] = False
            if folder_created:
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != PASS:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
