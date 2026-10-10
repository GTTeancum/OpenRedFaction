"""Parent-only original L11S3 startup -> ordinary HDD save -> fresh load.

The 2026-10-09 10:00 save succeeded; its observer rejected the expected N-2
sample. The original failed report remains immutable. --resume-saved permits
one fresh load of that exact pinned HDD with the same tested XBE/map or an explicit parent build manifest, without
another save or build. Resume and the corrected observer are source-only until
the parent's next batch. This wrapper never builds.
It calls extract-xiso directly and reuses native.run_guest serially. No
fixture, injected event, grant, moved spawn, traversal, host input or image.
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

ACTORS = (10636, 10637)
SAVE_FRAMES, LOAD_FRAMES = 120, 32
BODY_MASK, OBJECT_MASK = 0x99400001, 0x06000000
RFL_SHA256 = '86d24185992fb1380e53373b0b921481f70e1f602dbfc070b6c9b71310a999b8'
# Read-only pins taken from the actual 10:00 native save after its observer
# failure. Recovery is intentionally bounded to this producer and payload.
RESUME_SOURCE_COMMIT = 'aad0bdd2888dcec2a25a0c9e13dedcd5e120516f'
RESUME_PINS = {
    'verification.json': 'f690ed8f3caf1f1fd75b94c86cf5a23f706d87658350cc8ad5b790a0a3c1a231',
    'save/result.json': '76c6740071ee61b9f54256532517f2820100343f20091c9beb39efdd50619697',
    'save/xbox-world.rfwc': '34c15d8bbc0a1475bcfc8be4bc8bbca9cd1b41413ed29f4b398d69fc327b81e0',
    'private-save.qcow2': '129855eb9c0f311c52656c35f5443913a2a18b93e5ee1d348c5470e77139373b',
    'tested-default.xbe': 'fbd794525f403d8aea593d33fd7b64fa67e5a45e29c50b1cffc2efd8872ba6f0',
    'tested-main.map': '1e1f027a8f315e352fcb833a5e23ac772af259b84a8266a052b3a9470685fa53',
}
SYMBOLS = {
    'rf_scene_physics_state': 8, 'rf_scene_physics_npcs': 32,
    'rf_scene_npc_physics_restored': 8, 'rf_scene_startup_events': 9,
    'rf_scene_player_spawn_diagnostic': 19, 'rf_scene_setup_result': 4,
    'rf_scene_npc_checkpoint_reject_state': 6, 'rf_scene_world_load_reject': 3,
    'rf_scene_world_restore_reject': 8, 'rf_scene_npc_restore_reject': 6,
    'rf_scene_checkpoint_world_reject': 9,
    'rf_scene_world_snapshot_event_probe': 6, 'rf_scene_section_autosave': 4,
    'rf_scene_campaign_load_stage': 1, 'rf_scene_late_load': 8,
}


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def u32(data, at):
    demand(0 <= at <= len(data) - 4, 'Truncated checkpoint word')
    return struct.unpack_from('<I', data, at)[0]


def checksum(data):
    value = 2166136261
    for at, byte in enumerate(data):
        value = ((value ^ (0 if 12 <= at < 16 else byte)) * 16777619) & 0xffffffff
    return value


def floats(words):
    return list(struct.unpack('<' + 'f' * len(words), struct.pack('<' + 'I' * len(words), *words)))


def decode_npcs(payload):
    """Read actual guest RFWC/RFNC bytes only; never create or repair a save.

    RFNC1..14 have explicit base sizes; 13 adds 12 bytes and 14 adds another
    76 bytes after every row's variable payload. Unknown versions fail closed.
    This structural reader is not a replacement for the native semantic codec.
    """
    demand(308 <= len(payload) <= 110524 and payload[:4] == b'RFWC', 'Missing native RFWC payload')
    version = u32(payload, 4)
    demand(version in (1, 2, 3), 'Unsupported RFWC version')
    count = {1: 15, 2: 16, 3: 17}[version]
    demand(u32(payload, 8) == len(payload) and u32(payload, 12) == checksum(payload), 'RFWC size/checksum mismatch')
    demand(u32(payload, 16) == count and not any(payload[20:24] + payload[120:128]), 'RFWC header mismatch')
    demand(payload[56:120] == b'L11S3.rfl'.ljust(64, b'\0'), 'Wrong saved level')
    at, blob = 128 + count * 12, None
    for index in range(count):
        kind, offset, size = struct.unpack_from('<3I', payload, 128 + 12 * index)
        demand(kind == index + 1 and offset == at and size <= len(payload) - at, 'Malformed RFWC directory')
        if kind == 2:
            blob = payload[at:at + size]
        at += size
    demand(at == len(payload) and blob is not None and len(blob) >= 64 and blob[:4] == b'RFNC', 'Missing RFNC component')
    version, count = u32(blob, 4), u32(blob, 16)
    demand(1 <= version <= 14 and count <= 2048, 'Unsupported RFNC version/count')
    demand(u32(blob, 8) == len(blob) and u32(blob, 12) == checksum(blob), 'RFNC size/checksum mismatch')
    demand(not u32(blob, 20) and not u32(blob, 60) and blob[24:56] == payload[24:56], 'RFNC identity/reserved mismatch')
    base = (528, 540, 544, 548, 552, 564, 568, 572, 588, 600, 600, 600, 600, 600)[version - 1]
    at, previous, rows = 64, None, []
    for unused in range(count):
        demand(at + base <= len(blob), 'Truncated RFNC base row')
        uid = u32(blob, at)
        demand(previous is None or uid > previous, 'RFNC UIDs not strictly sorted')
        previous = uid
        animation = u32(blob, at + 540) if version >= 3 else 0
        move = u32(blob, at + 564) if version >= 7 else 0
        combat = u32(blob, at + 568) if version >= 8 else 0
        shots = u32(blob, at + 588) if version >= 10 else 0
        demand(move in (0, 168) and combat in (0, 40) and shots <= 16, 'Malformed RFNC optional lengths')
        animation_at = at + base + move + combat + shots * 24
        tail = animation_at + animation
        end = tail + (12 if version >= 13 else 0) + (76 if version >= 14 else 0)
        demand(end <= len(blob), 'Truncated RFNC variable row/tail')
        if animation:
            slots = u32(blob, animation_at + 16)
            demand(108 <= animation <= 300 and slots <= 16 and animation == 108 + slots * 12, 'Malformed RFNC animation span')
        movement = list(struct.unpack_from('<3I', blob, tail)) if version >= 13 else [0] * 3
        physics = list(struct.unpack_from('<19I', blob, tail + 12)) if version >= 14 else [0] * 19
        demand(physics[0] in (0, 1) and physics[1] in (0, 1), 'Malformed RFNC14 presence/marker')
        demand(not physics[2] & ~BODY_MASK and not physics[3] & ~OBJECT_MASK, 'Unknown RFNC14 mask bits')
        demand(physics[0] or not any(physics), 'Absent RFNC14 tail contains data')
        demand(all(math.isfinite(value) for value in floats(physics[4:])), 'Nonfinite RFNC14 vector')
        rows.append(dict(uid=uid, class_id=u32(blob, at + 4), retired=u32(blob, at + 8),
            health_bits=u32(blob, at + 20), position_bits=list(struct.unpack_from('<3I', blob, at + 28)),
            dead_pose=u32(blob, at + 552) if version >= 6 else 0,
            support_uid=u32(blob, at + 572) if version >= 9 else 0,
            movement_words=movement, physics_words=physics, row_bytes=end - at))
        at = end
    demand(at == len(blob), 'Trailing RFNC bytes')
    return dict(version=version, count=count, component_bytes=len(blob),
                component_sha256=hashlib.sha256(blob).hexdigest(), rows=rows)


def owner_samples(guest, frames):
    # scene.c presents frame N-1 but steps/samples only while frame+1<N.
    # Require the completed fixed-length run and exactly its final simulation
    # sample, rather than accepting an arbitrary age window. No row at N-1 is
    # produced. See docs/NPC-PHYSICS-CHECKPOINT.md for the source ordering.
    demand(frames >= 2 and guest.get('guest_phase') == 5 and
           guest.get('frames') == frames and guest.get('actor_frame_count') == frames and
           guest.get('replay_state') == [0, frames, frames, 0], 'Incomplete fixed-length owner observation')
    expected_frame = frames - 2
    raw = guest['extra']['rf_scene_physics_npcs']
    demand(len(raw) == 32, 'Malformed live owner sample span')
    result = {}
    for at in (0, 16):
        row = raw[at:at + 16]
        demand(row[0] in ACTORS and row[0] not in result and row[1] not in (0, 0xffffffff), 'Missing/ambiguous live frozen owner')
        demand(row[11] == 1 and row[2] & 0x98000000 == 0x18000000, 'Owner is not script-suspended')
        demand(row[12] == expected_frame,
               'Stale owner sample: UID %u frame %u, expected %u' % (row[0], row[12], expected_frame))
        demand(all(math.isfinite(value) for value in floats(row[4:11] + row[13:16])), 'Nonfinite live owner state')
        demand(all(value == 0 for value in floats(row[7:10] + row[13:16])), 'Frozen owner has linear/angular motion')
        result[row[0]] = row
    return result


def resume_saved(folder, xbe, mapping, sha256, consumer=None):
    """Read only this exact failed-observer batch; never repair its evidence."""
    for name, expected in RESUME_PINS.items():
        path = folder / name
        demand(path.is_file() and sha256(path) == expected, 'Resume evidence pin mismatch: ' + name)
    prior = json.loads((folder / 'verification.json').read_text())
    saved = json.loads((folder / 'save/result.json').read_text())
    demand(prior['status'] == 'CHECK_FAILED' and prior['error'] == 'RuntimeError: Stale owner sample' and
           prior['source_commit'] == RESUME_SOURCE_COMMIT and prior['source_status'] == '' and
           prior['phases'] == {'save': saved}, 'Resume is not the reviewed save-only observer failure')
    demand(prior['restoration_errors'] == [] and all(prior.get(key) is True for key in
           ('disc_flags_restored', 'exact_xbe_map_unchanged', 'original_iso_restored',
            'original_archives_unchanged', 'base_hdd_unchanged')), 'Prior save restoration was not clean')
    demand(prior['exact_inputs']['xbe'] == RESUME_PINS['tested-default.xbe'] and
           prior['exact_inputs']['map'] == RESUME_PINS['tested-main.map'], 'Original producer pins changed')
    expected = consumer if consumer is not None else dict(
        xbe=RESUME_PINS['tested-default.xbe'], map=RESUME_PINS['tested-main.map'])
    demand(sha256(xbe) == expected['xbe'] and sha256(mapping) == expected['map'],
           'Current XBE/map differ from the explicitly pinned consumer; no automatic staging')
    demand(saved['world_checkpoint_sha256'] == RESUME_PINS['save/xbox-world.rfwc'], 'Saved payload pin differs from guest capture')
    return prior, saved


def common_checks(guest, frames, spawn):
    checks = dict(
        native_success=guest.get('guest_phase') == 5,
        stock_64_mib=guest.get('memory_bytes') == 64 * 1024 * 1024,
        exact_neutral_replay=guest.get('frames') == frames and guest.get('replay_state', [])[1:] == [frames, frames, 0],
        memory_available=0 < guest.get('free_pages', 0) <= 16384,
        no_death_or_respawn=guest.get('player_life', [])[:3] == [0, 0, 0],
        no_transition=guest.get('level_transitions', [None])[0] == 0 and guest.get('level_request', [None])[0] == 0,
        original_spawn_and_basis=guest['extra']['rf_scene_player_spawn_diagnostic'][:13] == spawn,
        no_direct_setup=guest['extra']['rf_scene_setup_result'] == [0] * 4,
        authored_startup=guest['extra']['rf_scene_startup_events'][0] > 0,
        original_pair_freeze=guest['extra']['rf_scene_physics_state'][:5] == [2, 2, 0, 0, ACTORS[-1]] and
                             guest['extra']['rf_scene_physics_state'][7] == 0,
    )
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--resume-saved', type=Path,
                        help='Read-only prior 10:00 evidence directory; run only the 32-frame fresh load with -snapshot')
    parser.add_argument('--consumer-build', type=Path, help='Parent-created successful build manifest for a corrected consumer; producer save stays immutable')
    parser.add_argument('--seconds', type=int, default=600, help='Per guest process bound, 120..900 seconds')
    args = parser.parse_args()
    if not 120 <= args.seconds <= 900:
        parser.error('--seconds must be in 120..900')
    root, folder = args.root.resolve(), args.out.resolve()
    resume = args.resume_saved.resolve() if args.resume_saved is not None else None
    consumer = None
    if args.consumer_build is not None:
        if resume is None:
            parser.error('--consumer-build requires --resume-saved')
        consumer = json.loads(args.consumer_build.read_text())
        head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
        demand(consumer.get('status') == 'PASS_XBOX_BUILD' and consumer.get('source_commit') == head,
               'Consumer manifest must identify the current successfully built source')
        for key in ('xbe', 'map'):
            value = consumer.get(key, '')
            demand(len(value) == 64 and all(c in '0123456789abcdef' for c in value), 'Invalid consumer hash')
    if folder.exists() or folder == root or root in folder.parents:
        parser.error('--out must be a new directory outside the source repository')
    if resume is not None and (not resume.is_dir() or resume == root or root in resume.parents or
                               folder == resume or resume in folder.parents):
        parser.error('--resume-saved must be an existing external directory; --out must not be inside it')
    sys.path.insert(0, str(root / 'tools'))
    import xemu_native_world_save as native
    from xemu_fighter_grounded_exit import BorrowedSessionLock, sha256, write_json
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    from xemu_world_hdd import standalone
    from build_fragment_platform_fixture import read_entry
    from inspect_levels import inspect as inspect_level
    from inspect_triggers import inspect as inspect_triggers
    # Only the read-only archive helper is reused; no fixture main/build call.
    demand(native.ROOT.resolve() == root, 'Native runner belongs to a different tree')
    disc, iso = root / 'build/xbox/disc', root / 'build/xbox/redfaction-diagnostic.iso'
    xbe, mapping = disc / 'default.xbe', root / 'build/xbox/main.map'
    base = root / 'local/xemu-harness/pacing-base.qcow2'
    private_hdd = (resume if resume is not None else folder) / 'private-save.qcow2'
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (xbe, mapping, base, packer):
        demand(path.is_file(), 'Missing existing parent-built/owned input: ' + str(path))
    standalone(base)
    symbols = dict(SYMBOLS)
    if consumer is None:
        symbols.pop('rf_scene_world_restore_reject', None)
        symbols.pop('rf_scene_npc_restore_reject', None)
        symbols.pop('rf_scene_checkpoint_world_reject', None)
    for name in symbols:
        native.address(mapping.read_text(), name)
    payload = read_entry(root / 'Installed_Game/levels2.vpp', 'L11S3.rfl')
    demand(hashlib.sha256(payload).hexdigest() == RFL_SHA256, 'Original L11S3 bytes differ from the reviewed chain')
    meta = inspect_level(io.BytesIO(payload), dict(offset=0, size=len(payload), name='L11S3.rfl'))
    demand(meta['version'] == 180, 'Require original L11S3 version180')
    def section(kind):
        record = next(row for row in meta['sections'] if row['type'] == hex(kind))
        return payload[record['offset'] + 8:record['offset'] + 8 + record['size']]
    matches = [row for row in inspect_triggers(section(0x60000)) if row['uid'] == 12195]
    demand(len(matches) == 1, 'Original Auto12195 absent or ambiguous')
    trigger = matches[0]
    demand(trigger['flags'] == [0, 0, 0, 1, 0] and trigger['tail_flag'] == 0 and
           trigger['script'] == '' and trigger['links'] == [12214], 'Original startup eligibility differs')
    start = list(struct.unpack('<12I', section(0x70000)))
    spawn = [1] + start[:3] + start[6:12] + start[3:6]
    recipe = dict(level='L11S3.rfl', archive='levels2.vpp', rfl_sha256=RFL_SHA256,
        trigger=trigger, chain='Auto12195 -> zero-delay Delay12214 -> zero-delay Turn_Off_Physics10651 -> Eos10636/miner10637',
        source='docs/NPC-PHYSICS-CHECKPOINT.md; original byte-pinned archive',
        save_frames=SAVE_FRAMES, load_frames=LOAD_FRAMES,
        inputs='All-zero RFI6 records; unchanged authored spawn/basis; no direct setup',
        load_initialization='Ordinary fresh-scene auto startup runs before frame-zero load. Assignment telemetry separately establishes RFNC14 publication.')
    report = dict(status='NOT_RUN', recipe=recipe, phases={}, checks={},
        source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        source_status=subprocess.check_output(['git', 'status', '--short'], cwd=root, text=True),
        hdd=str(private_hdd), hdd_mode=('Existing pinned private saved HDD; fresh load uses -snapshot only' if resume is not None else
                                     'Private standalone copy of owned base; save writes only copy; fresh load uses -snapshot'),
        limits='No wake event, pre14 migration, full campaign, images, visual/audio or performance claim. '
               'All five saved RFNC14 vectors are inspected; existing live telemetry exposes only linear/angular vectors. '
               'Fresh load confirms ordinary load success, affected assignment count/latest owner, and both owners\' continued freeze/pose/health.')
    require_no_project_xemu(root)
    lock = SessionLock(root)
    lock.acquire()
    original_lock, originals = native.SessionLock, None
    staged = iso_moved = False
    try:
        require_no_project_xemu(root)
        folder.mkdir(parents=True)
        report['base_hdd_sha256'] = sha256(base)
        if resume is not None:
            prior, saved = resume_saved(resume, xbe, mapping, sha256, consumer)
            if consumer is not None:
                report['consumer_build'] = dict(consumer)
            demand(report['base_hdd_sha256'] == prior['base_hdd_sha256'], 'Owned base changed since the original save')
            demand(subprocess.check_output(['git', 'rev-parse', RESUME_SOURCE_COMMIT + '^{commit}'],
                   cwd=root, text=True).strip() == RESUME_SOURCE_COMMIT, 'Producer source commit is unavailable')
            report['resume_source'] = dict(directory=str(resume), original_status=prior['status'],
                source_commit=RESUME_SOURCE_COMMIT, pins=dict(RESUME_PINS),
                scope='Original failed report unchanged; save telemetry/bytes reused; one new native fresh-load process only')
            report['private_hdd_after_save_sha256'] = RESUME_PINS['private-save.qcow2']
        else:
            demand(shutil.disk_usage(folder).free >= base.stat().st_size + 2 * 1024**3, 'Insufficient private HDD space plus 2 GiB reserve')
            shutil.copyfile(base, private_hdd)
        standalone(private_hdd)
        demand(not os.path.samefile(private_hdd, base), 'Private HDD aliases the owned base')
        if resume is None:
            demand(sha256(private_hdd) == report['base_hdd_sha256'], 'Private HDD copy mismatch')
        write_json(folder / 'hdd-owner.json', dict(purpose='L11S3 ordinary frozen-NPC save batch', base=str(base),
                   disk=str(private_hdd), base_sha256=report['base_hdd_sha256'], snapshot_only=resume is not None))
        archives = {}
        for archive in sorted(disc.glob('*.vpp')):
            if archive.name == 'scene-fixture.vpp':
                continue
            source = root / 'Installed_Game' / archive.name
            demand(source.is_file(), 'Disc contains non-original archive: ' + archive.name)
            archives[archive.name] = sha256(source)
            demand(sha256(archive) == archives[archive.name], 'Disc archive differs from original: ' + archive.name)
        demand({'levels2.vpp', 'tables.vpp', 'meshes.vpp'} <= archives.keys(), 'Missing original level/class/mesh archives')
        report['original_archive_sha256'] = archives
        names = set(native.FLAGS) | {'scene-fixture.vpp', 'player-control.flag', 'scene-preview.flag'}
        immutable = {'geomod-template.bin', 'driller-single.bin', 'driller-double.bin'}
        names |= {p.name for p in disc.iterdir() if p.is_file() and (p.name.startswith('campaign-') or
                  (p.suffix in ('.flag', '.bin', '.txt') and p.name not in immutable))}
        originals = {n: (disc / n).read_bytes() if (disc / n).exists() else None for n in sorted(names)}
        write_json(folder / 'disc-restore.json', {n: data.hex() if data is not None else None for n, data in originals.items()})
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
        (disc / 'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64, b'\0') + b'L11S3.rfl'.ljust(64, b'\0'))
        native.SessionLock = lambda unused_root: BorrowedSessionLock(lock)

        def run_phase(name, frames):
            demand(resume is None or name == 'load', 'Resume cannot launch another save process')
            for flag in ('world-hdd-save.flag', 'world-hdd-load.flag'):
                (disc / flag).unlink(missing_ok=True)
            (disc / ('world-hdd-' + name + '.flag')).write_bytes(b'1')
            (disc / 'player-replay.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(frames * 48))
            iso.unlink(missing_ok=True)
            with (folder / (name + '-pack-only.log')).open('wb') as log:
                subprocess.run([str(packer), '-c', str(disc), str(iso)], cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
            demand(sha256(xbe) == report['exact_inputs']['xbe'] and sha256(mapping) == report['exact_inputs']['map'], 'Exact XBE/map changed')
            report[name + '_iso_sha256'] = sha256(iso)
            try:
                guest = native.run_guest(folder, name, private_hdd, frames, args.seconds,
                    snapshot=name == 'load', extra_symbols=symbols,
                    capture_world=name == 'save', allow_guest_error=True, allow_player_dead=True)
            except BaseException:
                # The native runner writes terminal diagnostics even if capture
                # fails. Preserve that result; never manufacture missing payload.
                result_path = folder / name / 'result.json'
                if result_path.is_file():
                    report['phases'][name] = json.loads(result_path.read_text())
                raise
            report['phases'][name] = guest
            checks = common_checks(guest, frames, spawn)
            report['checks'][name] = checks
            demand(all(checks.values()), name + ' failed checks: ' + ', '.join(key for key, passed in checks.items() if not passed))
            return guest

        if resume is None:
            saved = run_phase('save', SAVE_FRAMES)
        else:
            demand(prior['recipe'] == recipe, 'Original level/startup/save recipe changed')
            demand(prior['original_archive_sha256'] == archives, 'Original archive set changed')
            report['phases']['save'] = saved
            report['checks']['save'] = common_checks(saved, SAVE_FRAMES, spawn)
            demand(all(report['checks']['save'].values()), 'Reused native save failed common checks')
        state = saved['checkpoint_state']
        demand(state[9] == 1 and state[8] == 0 and state[3] == 0 and 320 <= state[4] <= 110524, 'Ordinary save failed; load is not authorized to continue')
        demand(saved['storage_state'][1] == 0 and saved['storage_state'][7] & 4, 'Native storage did not confirm save publication')
        saved_bytes = ((resume if resume is not None else folder) / 'save/xbox-world.rfwc').read_bytes()
        demand(len(saved_bytes) == state[4], 'Actual saved payload length mismatch')
        npc = decode_npcs(saved_bytes)
        report['saved_npc_component'] = npc
        demand(npc['version'] == 14, 'Affected native save did not select RFNC14')
        affected = {row['uid']: row for row in npc['rows'] if row['physics_words'][0]}
        demand(set(affected) == set(ACTORS), 'RFNC14 affected owners differ from original linked pair')
        before = owner_samples(saved, SAVE_FRAMES)
        for uid in ACTORS:
            row, live = affected[uid], before[uid]
            p = row['physics_words']
            demand(not row['retired'] and not row['dead_pose'] and p[:2] == [1, 1], 'Frozen NPC saved as retired/dead/absent')
            demand(p[2] == live[2] & BODY_MASK and p[3] == live[3] & OBJECT_MASK and
                   p[4:7] == live[7:10] and p[7:10] == live[13:16] and
                   row['position_bits'] == live[4:7] and row['health_bits'] == live[10], 'RFNC14 row differs from actual pre-save owner')
        report['before_owners'] = before
        if resume is None:
            report['private_hdd_after_save_sha256'] = sha256(private_hdd)
        else:
            demand(npc == prior['saved_npc_component'], 'Reused RFNC14 rows differ from original evidence')
        # Exactly one load process, only after actual save and row checks pass.
        loaded = run_phase('load', LOAD_FRAMES)
        state = loaded['checkpoint_state']
        demand(state[8] == 1 and state[9] == 0 and state[0] == 0 and state[1] == len(saved_bytes), 'Ordinary fresh load failed')
        demand(loaded['storage_state'][1] == 0 and loaded['storage_state'][4:7] == saved['storage_state'][4:7], 'Fresh load selected different native generation/slot/length')
        after = owner_samples(loaded, LOAD_FRAMES)
        report['after_owners'] = after
        for uid in ACTORS:
            pre, post, row = before[uid], after[uid], affected[uid]
            p = row['physics_words']
            demand(post[2] & BODY_MASK == p[2] and post[3] & OBJECT_MASK == p[3] and
                   post[4:11] == pre[4:11] and post[11] == pre[11] and post[13:16] == pre[13:16], 'Fresh-loaded frozen owner changed pose, health, flags or motion')
        last = affected[ACTORS[-1]]['physics_words']
        demand(loaded['extra']['rf_scene_npc_physics_restored'] ==
               [2, 2, ACTORS[-1], after[ACTORS[-1]][1], 1, 1, last[2], last[3]], 'RFNC14 assignment telemetry did not confirm both suspended owners')
        report['status'] = 'PASS_ORIGINAL_STARTUP_FROZEN_NPC_SAVE_FRESH_LOAD'
    except BaseException as error:
        report.update(status='CHECK_FAILED', error=type(error).__name__ + ': ' + str(error))
        raise
    finally:
        native.SessionLock = original_lock
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
                report['disc_flags_restored'] = all(((disc / n).read_bytes() if (disc / n).exists() else None) == data for n, data in originals.items())
            if 'exact_inputs' in report:
                pins = report['exact_inputs']
                report['exact_xbe_map_unchanged'] = sha256(xbe) == pins['xbe'] and sha256(mapping) == pins['map']
                report['original_iso_restored'] = (sha256(iso) if iso.exists() else None) == pins['iso']
            if 'original_archive_sha256' in report:
                report['original_archives_unchanged'] = all(sha256(disc / n) == value and sha256(root / 'Installed_Game' / n) == value for n, value in report['original_archive_sha256'].items())
            if 'base_hdd_sha256' in report:
                report['base_hdd_unchanged'] = sha256(base) == report['base_hdd_sha256']
            if 'private_hdd_after_save_sha256' in report:
                report['fresh_load_hdd_unchanged'] = sha256(private_hdd) == report['private_hdd_after_save_sha256']
            if 'resume_source' in report:
                report['original_save_evidence_unchanged'] = all(sha256(resume / name) == expected
                    for name, expected in RESUME_PINS.items() if name != 'private-save.qcow2')
            report['restoration_errors'] = errors
            if errors or any(report.get(key) is False for key in ('disc_flags_restored', 'exact_xbe_map_unchanged',
                    'original_iso_restored', 'original_archives_unchanged', 'base_hdd_unchanged', 'fresh_load_hdd_unchanged',
                    'original_save_evidence_unchanged')):
                report['status'] = 'CHECK_FAILED'
            if folder.exists():
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            lock.close()
    if report['status'] != 'PASS_ORIGINAL_STARTUP_FROZEN_NPC_SAVE_FRESH_LOAD':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
