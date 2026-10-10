"""Parent-only original L1S1 startup admission after one hourly Xbox build.

One 120-frame all-neutral stock 64 MiB snapshot run. Pack the manifest-pinned
XBE without make; never create a gameplay fixture, alter original assets,
grant inventory, inject events, save/load, capture images/audio, measure FPS,
send host input, or retry. This is startup/table admission only.
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

FRAMES = 120
SYMBOLS = {'rf_scene_player_spawn_diagnostic': 19, 'rf_scene_weapon_supply': 4}
NATIVE_SYMBOLS = (
    'rf_diagnostic', 'rf_scene_world_checkpoint_state',
    'rf_xbox_checkpoint_storage_state', 'rf_scene_player_life',
    'rf_scene_checkpoint_world_route_contact', 'rf_xbox_level_transitions',
    'rf_player_replay_diagnostic', 'rf_scene_actor_frame_count',
    'rf_scene_campaign_load_stage', 'rf_scene_follow_level_exits',
    'rf_scene_level_transition',
)
LIMITS = ('Original L1S1 startup and weapon-table admission only; no unarmed-pickup, '
          'grenade-selection, save/load, gameplay, audio-output, visual or FPS pass.')


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def original_recipe(root, read_entry, inspect_level):
    data = read_entry(root / 'Installed_Game/levels1.vpp', 'L1S1.rfl')
    meta = inspect_level(io.BytesIO(data), dict(offset=0, size=len(data), name='L1S1.rfl'))
    demand(meta['version'] == 180, 'Require original version 180 L1S1')
    starts = [row for row in meta['sections'] if row['type'] == '0x70000']
    demand(len(starts) == 1 and starts[0]['size'] == 48, 'Missing/ambiguous original player start')
    start = list(struct.unpack_from('<12I', data, starts[0]['offset'] + 8))
    # level.c converts disk forward/right/up into runtime right/up/forward.
    return dict(level='L1S1.rfl', archive='levels1.vpp',
        rfl_sha256=hashlib.sha256(data).hexdigest(), rfl_bytes=len(data),
        spawn_words=[1] + start[:3] + start[6:12] + start[3:6], frames=FRAMES,
        input='RFI6: 120 records of 48 zero bytes; all movement/look/buttons neutral.',
        limits=LIMITS)


def evaluate(guest, recipe):
    extra = guest.get('extra', {})
    supply = extra.get('rf_scene_weapon_supply', [])
    checks = dict(
        stock64=guest.get('memory_bytes') == 64 * 1024 * 1024,
        terminal_complete=guest.get('guest_phase') == 5 and guest.get('frames') == FRAMES,
        exact_neutral_replay=guest.get('replay_state', [])[1:] == [FRAMES, FRAMES, 0],
        no_death_or_respawn=guest.get('player_life', [])[:3] == [0, 0, 0],
        no_transition=guest.get('level_transitions', [None])[0] == 0 and
                      guest.get('level_request', [None])[0] == 0,
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        original_spawn_and_basis=extra.get('rf_scene_player_spawn_diagnostic', [])[:13] == recipe['spawn_words'],
        weapon_tables_admitted=len(supply) == 4 and 0 < supply[0] <= 64 and
                               0 < supply[1] <= supply[0] and supply[2] > 0,
    )
    # scene.c publishes supply only after scene_grenade_empty_load succeeds.
    # The private preference-ready owner is reset by cleanup; do not sample it.
    failed = [name for name, passed in checks.items() if not passed]
    return dict(status='CHECK_FAILED' if failed else 'PASS_ORIGINAL_STARTUP_ADMISSION',
                checks=checks, failed_checks=failed)


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
    addresses = {name: exact_address(map_text, name) for name in NATIVE_SYMBOLS + tuple(SYMBOLS)}
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
        (disc / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        recording = b'RFI6' + struct.pack('<I', 48) + bytes(48 * FRAMES)
        (disc / 'player-replay.bin').write_bytes(recording)
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
            extra_symbols=SYMBOLS, allow_guest_error=True, capture_world=False, measure_fps=False)
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
    if report['status'] != 'PASS_ORIGINAL_STARTUP_ADMISSION':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
