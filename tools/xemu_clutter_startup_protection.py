"""Parent-only, pack-only original L3S4 neutral startup observation.

Run once after the parent's 08:00 Xbox compile. Reuses native.run_guest.
No build, altered archive, event injection, save/load, movement or capture.
Prepared for the parent hourly batch; no runtime success is implied.
"""
import argparse
import hashlib
import io
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
RFL_HASH = '29d23e651514e6c7ad8fbc328218e72376d177e297aa5d73a2840a0e91e0241f'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if not 120 <= args.seconds <= 900:
        parser.error('--seconds must be in 120..900')
    root, folder = args.root.resolve(), args.out.resolve()
    if folder.exists():
        parser.error('Evidence directory already exists; do not repeat an uncertain run')
    sys.path.insert(0, str(root / 'tools'))
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    from inspect_triggers import inspect as inspect_triggers
    # Only read_entry is reused from the fixture module; its main is never called.
    if native.ROOT.resolve() != root:
        raise RuntimeError('Native runner belongs to a different source tree')
    disc = root / 'build/xbox/disc'
    iso = root / 'build/xbox/redfaction-diagnostic.iso'
    xbe, mapping = disc / 'default.xbe', root / 'build/xbox/main.map'
    hdd = root / 'local/xemu-harness/pacing-base.qcow2'
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    symbols = {'rf_scene_clutter_invulnerability': 6,
               'rf_scene_player_spawn_diagnostic': 19,
               'rf_scene_startup_events': 9}
    for path in (xbe, mapping, hdd, packer):
        if not path.is_file():
            raise RuntimeError('Missing existing parent-built/owned input: ' + str(path))
    standalone(hdd)
    for name in symbols:
        native.address(mapping.read_text(), name)
    payload = read_entry(root / 'Installed_Game/levels1.vpp', 'L3S4.rfl')
    if hashlib.sha256(payload).hexdigest() != RFL_HASH:
        raise RuntimeError('Original L3S4 bytes differ from the documented authored chain')
    meta = inspect_level(io.BytesIO(payload), dict(offset=0, size=len(payload), name='L3S4.rfl'))
    if meta['version'] != 180:
        raise RuntimeError('Require the original version180 L3S4')
    def section(kind):
        record = next(row for row in meta['sections'] if row['type'] == hex(kind))
        return payload[record['offset'] + 8:record['offset'] + 8 + record['size']]
    matches = [row for row in inspect_triggers(section(0x60000)) if row['uid'] == 1007]
    if len(matches) != 1:
        raise RuntimeError('Authored Trigger1007 absent or ambiguous')
    trigger = matches[0]
    if (trigger['flags'] != [0, 0, 0, 1, 0] or trigger['tail_flag'] != 0 or
            trigger['script'] != '' or trigger['links'] != [8478, 1004, 9665, 9666, 9696]):
        raise RuntimeError('Authored startup eligibility/links differ from reviewed evidence')
    start_words = list(struct.unpack('<12I', section(0x70000)[:48]))
    # Source level.c changes disk forward/right/up to runtime right/up/forward.
    expected_spawn = [1] + start_words[:3] + start_words[6:12] + start_words[3:6]
    recipe = dict(level='L3S4.rfl', archive='levels1.vpp', rfl_sha256=RFL_HASH,
        trigger=trigger, event=dict(uid=8478, type=24, delay=0, duration=0, links=[8350]),
        target=dict(uid=8350, class_name='Console_Small01'),
        frames=FRAMES, inputs='120 all-zero RFI6 records; unchanged authored spawn/basis',
        provenance='docs/CLUTTER-INVULNERABILITY.md plus byte-pinned original RFL',
        timer_scope='Trigger cooldown does not defer its first startup callback; event delay/duration are zero.')
    require_no_project_xemu(root)
    report = dict(status='NOT_RUN', recipe=recipe,
        source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        hdd=str(hdd), hdd_mode='existing standalone owned test HDD, XEMU -snapshot',
        limits='Only original authored startup callback and its bit4 change. Does not establish '
               'damage rejection, Slay/When_Dead dispatch, OFF, save/restore, visuals or campaign progression.')
    lock = SessionLock(root)
    lock.acquire()
    old_lock = native.SessionLock
    originals = None
    iso_moved = staged = False
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        archive_hashes = {}
        for archive in sorted(disc.glob('*.vpp')):
            if archive.name == 'scene-fixture.vpp':
                continue  # Saved/removed with diagnostic selectors below.
            original = root / 'Installed_Game' / archive.name
            source_hash = sha256(original) if original.is_file() else None
            if source_hash is None or sha256(archive) != source_hash:
                raise RuntimeError('Disc archive is not an exact original input: ' + archive.name)
            archive_hashes[archive.name] = source_hash
        if not {'levels1.vpp', 'tables.vpp', 'meshes.vpp'} <= archive_hashes.keys():
            raise RuntimeError('Missing required original level/class/mesh archives')
        report['original_archive_sha256'] = archive_hashes
        names = set(native.FLAGS) | {'scene-fixture.vpp', 'player-control.flag', 'scene-preview.flag'}
        # Preserve/remove every diagnostic selector, while retaining required
        # generated immutable crater/driller geometry data and original assets.
        immutable_bins = {'geomod-template.bin', 'driller-single.bin', 'driller-double.bin'}
        names |= {p.name for p in disc.iterdir() if p.is_file() and
                  (p.name.startswith('campaign-') or
                   (p.suffix in ('.flag', '.bin', '.txt') and p.name not in immutable_bins))}
        originals = {name: (disc / name).read_bytes() if (disc / name).exists() else None
                     for name in sorted(names)}
        write_json(folder / 'disc-restore.json', {n: data.hex() if data is not None else None
                                                for n, data in originals.items()})
        report['exact_inputs'] = dict(xbe=sha256(xbe), map=sha256(mapping),
                                     iso=sha256(iso) if iso.exists() else None)
        shutil.copyfile(xbe, folder / 'tested-default.xbe')
        shutil.copyfile(mapping, folder / 'tested-main.map')
        write_json(folder / 'recipe.json', recipe)
        if iso.exists():
            iso.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in originals:
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L3S4.rfl'.ljust(64, b'\0'))
        (disc / 'scene-preview.flag').write_bytes(b'')
        (disc / 'player-control.flag').write_bytes(b'')
        (disc / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(FRAMES * 48))
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        pins = report['exact_inputs']
        if sha256(xbe) != pins['xbe'] or sha256(mapping) != pins['map']:
            raise RuntimeError('Exact XBE/map changed while packaging')
        report['tested_iso_sha256'] = sha256(iso)
        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)
        guest = native.run_guest(folder, 'native', hdd, FRAMES, args.seconds,
            snapshot=True, extra_symbols=symbols, allow_guest_error=True, allow_player_dead=True)
        report['native'] = guest
        callback = guest['extra']['rf_scene_clutter_invulnerability']
        report['callback'] = dict(zip(('accepted', 'uid', 'handle', 'enabled', 'before', 'after'), callback))
        checks = dict(stock_64_mib=guest.get('memory_bytes') == 64 * 1024 * 1024,
            exact_neutral_replay=guest.get('guest_phase') == 5 and guest.get('frames') == FRAMES and
                                 guest.get('replay_state', [])[1:] == [FRAMES, FRAMES, 0],
            no_death_or_respawn=guest.get('player_life', [])[:3] == [0, 0, 0],
            no_transition=guest.get('level_transitions', [None])[0] == 0 and
                          guest.get('level_request', [None])[0] == 0,
            memory_available=0 < guest.get('free_pages', 0) <= 16384,
            original_spawn_and_basis=guest['extra']['rf_scene_player_spawn_diagnostic'][:13] == expected_spawn,
            actual_target_callback=callback[0] >= 1 and callback[1] == 8350 and
                                   callback[2] not in (0, 0xffffffff) and callback[3] == 1,
            protection_was_off=(callback[4] & 4) == 0,
            changed_only_bit4=callback[5] == (callback[4] | 4),
            ordinary_startup_dispatched=guest['extra']['rf_scene_startup_events'][0] > 0)
        report['checks'] = checks
        report['failed_checks'] = [name for name, passed in checks.items() if not passed]
        report['status'] = 'CHECK_FAILED' if report['failed_checks'] else 'PASS_AUTHORED_STARTUP_INVULNERABILITY'
    except BaseException as error:
        report.update(status='CHECK_FAILED', error=type(error).__name__ + ': ' + str(error))
        raise
    finally:
        native.SessionLock = old_lock
        errors = []
        try:
            if staged:
                for name, data in originals.items():
                    try:
                        if data is None:
                            (disc / name).unlink(missing_ok=True)
                        else:
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
            if originals is not None:
                report['disc_flags_restored'] = all(
                    ((disc / n).read_bytes() if (disc / n).exists() else None) == data
                    for n, data in originals.items())
            if 'exact_inputs' in report:
                pins = report['exact_inputs']
                report['exact_xbe_map_unchanged'] = sha256(xbe) == pins['xbe'] and sha256(mapping) == pins['map']
                report['original_iso_restored'] = (sha256(iso) if iso.exists() else None) == pins['iso']
            if 'original_archive_sha256' in report:
                report['original_archives_unchanged'] = all(
                    sha256(disc / n) == value and sha256(root / 'Installed_Game' / n) == value
                    for n, value in report['original_archive_sha256'].items())
            report['restoration_errors'] = errors
            if errors or any(report.get(name) is False for name in
                    ('disc_flags_restored', 'exact_xbe_map_unchanged', 'original_iso_restored', 'original_archives_unchanged')):
                report['status'] = 'CHECK_FAILED'
            if folder.exists():
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != 'PASS_AUTHORED_STARTUP_INVULNERABILITY':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
