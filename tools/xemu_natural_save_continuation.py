"""Explicit, fail-closed admission for one separately identified saved consumer.

Candidate source only. The ordinary natural-save caller retains its V1 path.
No work occurs at import beyond declarations. No archive creation, networking,
HDD reconstruction, producer launch, or retry is implemented here.
"""
import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import zipfile

ROOT = Path('/workspace/shared/OpenRedFaction-attack')
PRODUCER = Path('/workspace/shared/rf-natural-save-producer-20261011-0800')
PRODUCER_COMMIT = 'e1d2ed5ae4e4c8f05586d26385d944d4e221e700'
PRODUCER_TREE = 'f86f74e23b4389d262d2c03b65e5eda9c685cb6b'
MANIFEST_SHA = '2166df6572729ff0d009dc3f3de0dd0894a05683d617915689b494ab2e584b31'
VERIFICATION_SHA = '2a590d2d901bdc235f1f7299ffbd81a7978d3114d507bac4b95b7938aad5ad54'
BUILD_SHA = '4d3a36edeff1af48a056f785ca8769aa200da0440e807ccece6d1f9b263237c5'
XBE_SHA = '809b98a9cba6e21b3e8a00826f156c0e87fbc788914fbf0bfbfc47c2d9cdb4fd'
MAP_SHA = '14dc720e8959b0a041ab27a41f633a69fce0298ed5599bc350367b10a4d84ef7'
HDD_SHA = '16f25163e1e6a0dbf41ee0542f7365af6cb2db9a468b3e26c497e01b77630cf1'
HDD_BYTES = 3047882752
PAYLOAD_SHA = 'a8ca6221c29595943ed481637099a8576f975afc05f82c6d898cd1604158f2a6'
DESIGN_SHA = '367464c6609229361a1f04fce73a382d75b956b28b30c6fe079641706af17ade'
STRUCTURAL_POLICY_SHA = '5ed12e4c0ab2c276a6675b458f9fe2be605b9501409859ba2cc2f9ed493722e0'
ACTUAL09_REUSE_POLICY_SHA = '54faa9fe6e37e64d005d963f2c7e72b0ab6a7f1b6b8b3461e117c9769655b1b0'
ARCHIVE_DESIGN_SHA = 'cadbe783c1ee82cf27c1cde7d261e6428365bb9e16858c3101f5a4b84db9b149'
ARCHIVE_SCOPE_SHA = 'decb7dd6a393219348069c909f0f9e31e8708f961646db9a81dd594f585401e2'
WORKSPACE_BUILD_PATH = Path('/workspace/shared/rf-20261011-0900-build.json')
WORKSPACE_BUILD_SHA = '21d1f121b788053bd4e91d332eefcacae0cc5ffc5e46211260a981ccde3b76e9'
PROOF_SHA = 'c1e27523f4a7912f9684d935b5af3c5c34a90cfd3de84fd4f11b32994af81ece'
PROOF_BYTES = 7433415
PROOF_INVENTORY_SHA = '730002b25b7468f695fcc7213c5acd3ed17f0c7f27e269e000cfb480dbb86014'
PROOF_CREATE_SHA = '2dce62f97870fcebd0d4f5e38c068b89ee4351b9f263eb6e2a26abf7fe5d1e8e'
PROOF_READBACK_RECORD_SHA = 'dadcc70b3ad41db6c1d6b9a5d004327857265eb12a27516cd7d0be6fc9142f39'
PROOF_RAW_SERVICE_SHA = 'eb3201bbed00e1d172fd05d8575cd63367f9ac64e6296f84e634eacd64a6d765'
ENVIRONMENT_SOURCE = '/workspace/shared/recovery/toolchain/environment.sh'
PROOF_IDENTITY = dict(library_file_id='libfile_8bda5d435de081918238bd3cce5a21d2',
    file_id='file_000000007a4482309b1c13c670eba4dd',
    file_name='OpenRedFaction-hourly-proof-20261011-0800.zip', version=0,
    sha256=PROOF_SHA, bytes=PROOF_BYTES)
PROOF_LOCAL = Path('/workspace/shared/OpenRedFaction-hourly-proof-20261011-0800.zip')
PROOF_READBACK = Path('/workspace/scratch/780c66634d06/actual08-terminal-proof-readback/OpenRedFaction-hourly-proof-20261011-0800.zip')
RAW_STAGING = Path('/workspace/scratch/780c66634d06/rf-natural-save-backup-20261011-0800/OpenRedFaction-New-L3S1-Natural-Save-20261011-0800.qcow2')
RESERVE = 2 * 1024**3
REPRESENTATION = 'zip64-deflate-opaque-complete-qcow2'
ARTIFACT_KEYS = {'role', 'library_file_id', 'file_id', 'file_name', 'version',
                 'sha256', 'bytes', 'local_path', 'readback_path'}
FILE_KEYS = {'path', 'sha256', 'bytes', 'device', 'inode'}
RAW_KEYS = FILE_KEYS | {'read_only'}
SOURCE_REQUIRED = {
    'tools/xemu_natural_save_gameplay.py', 'tools/xemu_natural_save_continuation.py',
    'tools/xemu_xbe_structural_admission.py',
    'tools/xemu_native_world_save.py', 'tools/xemu_fighter_grounded_exit.py',
    'tools/xemu_original_startup_admission.py', 'tools/xemu_guest_snapshot.py',
    'tools/xemu_original_switch_sound.py', 'tools/xemu_host.py',
    'tools/xemu_session_guard.py', 'tools/xemu_world_hdd.py', 'tools/xemu_smoke.py',
    'tools/build_fragment_platform_fixture.py', 'tools/inspect_levels.py', 'tools/inspect_geometry.py',
    'tools/build-xbox.sh', 'platforms/xbox/Makefile',
}
FIXED = {
    'geomod-template.bin': 'd489dc141999833ad88fa93dc68b686b0ecb5a864c8cb69ed78ea98c97450dab',
    'driller-double.bin': 'fd013c70a8bed5f64a456a9e29cacc6e3a3d930701bef0b39c3a26e8abcae4f3',
    'driller-single.bin': '2fac25f24f243f21ec453ff07bfbfb8c18630e1945f74f4e09bf154b76face41',
}


def require(ok, why):
    if not ok:
        raise RuntimeError('Continuation admission: ' + why)


def keys(value, names, why):
    require(type(value) is dict and set(value) == set(names), why + ': unexpected keys/type')
    return value


def number(value, why, positive=False):
    require(type(value) is int and value >= (1 if positive else 0), why + ': invalid integer')
    return value


def hash_value(value, why, length=64):
    require(type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value),
            why + ': invalid exact hash')
    return value


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False,
                      allow_nan=False).encode('utf-8')


def sha_bytes(value):
    return hashlib.sha256(value).hexdigest()


def same_json(left, right):
    """Unlike Python equality, JSON booleans never compare equal to integers."""
    return canonical(left) == canonical(right)


def path_checked(value, exists=True):
    require(type(value) in (str, Path) or isinstance(value, Path), 'Invalid path type')
    path = Path(value)
    require(path.is_absolute() and '..' not in path.parts and str(path) == str(value),
            'Require canonical absolute path: ' + str(value))
    for component in reversed((path,) + tuple(path.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            require(not exists, 'Missing path: ' + str(component))
            continue
        require(not stat.S_ISLNK(info.st_mode), 'Symlink component: ' + str(component))
    if exists:
        require(path.exists(), 'Missing path: ' + str(path))
    return path


def regular(value, readonly=False):
    path = path_checked(value)
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode), 'Not regular file: ' + str(path))
    require(not readonly or not info.st_mode & 0o222, 'Writable protected file: ' + str(path))
    return info


def digest(value):
    path = path_checked(value)
    before = regular(path)
    result = hashlib.sha256()
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW), 'rb') as stream:
        opened = os.fstat(stream.fileno())
        require((opened.st_dev, opened.st_ino) == (before.st_dev, before.st_ino),
                'File replaced before read: ' + str(path))
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            result.update(block)
        after_fd = os.fstat(stream.fileno())
    after = regular(path)
    identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mode, s.st_mtime_ns, s.st_ctime_ns)
    require(identity(before) == identity(after_fd) == identity(after),
            'File changed while hashing: ' + str(path))
    return result.hexdigest()


def strict_json(data, limit=262144):
    require(type(data) is bytes and len(data) <= limit, 'Oversized JSON')
    def pairs(items):
        out = {}
        for name, value in items:
            require(name not in out, 'Duplicate JSON key: ' + name)
            out[name] = value
        return out
    def invalid(value):
        raise RuntimeError('Continuation admission: nonfinite JSON ' + value)
    def finite(value):
        number_value = float(value)
        require(math.isfinite(number_value), 'Nonfinite JSON number')
        return number_value
    return json.loads(data.decode('utf-8'), object_pairs_hook=pairs,
                      parse_constant=invalid, parse_float=finite)


def read_json(value, expected=None, limit=262144):
    path = path_checked(value)
    before = regular(path)
    require(before.st_size <= limit, 'Oversized JSON file: ' + str(path))
    identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mode, s.st_mtime_ns, s.st_ctime_ns)
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW), 'rb') as stream:
        opened = os.fstat(stream.fileno())
        require(identity(opened) == identity(before) and stat.S_ISREG(opened.st_mode),
                'JSON replaced before read: ' + str(path))
        data = stream.read(limit + 1)
        require(len(data) <= limit and len(data) == opened.st_size,
                'Oversized or changed JSON file: ' + str(path))
        after_fd = os.fstat(stream.fileno())
    require(identity(before) == identity(after_fd) == identity(regular(path)),
            'JSON changed during bounded read: ' + str(path))
    if expected is not None:
        require(sha_bytes(data) == hash_value(expected, 'Expected JSON hash'),
                'JSON bytes differ: ' + str(path))
    return strict_json(data, limit), data


def file_record(value, expected_hash=None, expected_bytes=None, readonly=False):
    path = path_checked(value)
    before = regular(path, readonly)
    found_hash = digest(path)
    after = regular(path, readonly)
    require((before.st_dev, before.st_ino, before.st_mode, before.st_size) ==
            (after.st_dev, after.st_ino, after.st_mode, after.st_size), 'Changed file identity')
    if expected_hash is not None:
        require(found_hash == hash_value(expected_hash, 'File hash'), 'File hash mismatch: ' + str(path))
    if expected_bytes is not None:
        require(after.st_size == number(expected_bytes, 'File bytes'), 'File size mismatch: ' + str(path))
    return dict(path=str(path), sha256=found_hash, bytes=after.st_size,
                device=after.st_dev, inode=after.st_ino, mode=stat.S_IMODE(after.st_mode))


def independent(records):
    require(len({r['path'] for r in records}) == len(records), 'Independent paths overlap')
    require(len({(r['device'], r['inode']) for r in records}) == len(records),
            'Independent files share inode')


def unchanged(records):
    """Final-restoration predicate; does not hide failures as success."""
    try:
        for prior in records:
            current = file_record(prior['path'], prior['sha256'], prior['bytes'])
            if current != prior:
                return False
        return True
    except (OSError, RuntimeError, ValueError):
        return False


def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root)


def tree(root, revision):
    hash_value(revision, 'Git revision', 40)
    result = {}
    for row in git(root, 'ls-tree', '-rz', '--full-tree', revision).split(b'\0'):
        if not row:
            continue
        head, name_bytes = row.split(b'\t', 1)
        mode, kind, obj = head.decode('ascii').split(' ')
        name = name_bytes.decode('utf-8')
        safe_member(name)
        require(mode in ('100644', '100755') and kind == 'blob', 'Nonregular tracked entry: ' + name)
        hash_value(obj, 'Git object', 40)
        require(name not in result, 'Duplicate tree path')
        result[name] = dict(mode=mode, object=obj, sha256=sha_bytes(git(root, 'cat-file', 'blob', obj)))
    return result


def safe_member(name):
    require(type(name) is str and name and '\\' not in name and '\x00' not in name,
            'Unsafe member name')
    path = PurePosixPath(name)
    require(not path.is_absolute() and '..' not in path.parts and '.' not in path.parts and
            str(path) == name and not name.endswith('/'), 'Noncanonical member name: ' + name)
    return name


def validate_tree_entry(value):
    if value is None:
        return
    keys(value, {'mode', 'object', 'sha256'}, 'Tree entry')
    require(value['mode'] in ('100644', '100755'), 'Unsupported tracked mode')
    hash_value(value['object'], 'Tree object', 40)
    hash_value(value['sha256'], 'Tree blob hash')


def structural_module():
    # Call only after validate_sources has checked all explicit module bytes.
    sys.path.insert(0, str(ROOT / 'tools'))
    import xemu_xbe_structural_admission
    return xemu_xbe_structural_admission


def protected_unchanged(ctx):
    if not unchanged(ctx['protected_records']):
        return False
    try:
        if 'dependency_records' in ctx:
            structural = structural_module()
            current = structural.dependency_records(sys.modules[__name__])
            if not same_json(current, ctx['dependency_records']):
                return False
        if 'timed_artifact_observations' in ctx:
            if not structural_module().timed_artifacts_unchanged(sys.modules[__name__], ctx['timed_artifact_observations']):
                return False
        return True
    except (OSError, RuntimeError, ValueError):
        return False


def validate_sources(contract):
    root = path_checked(contract['root'])
    require(root == ROOT, 'Wrong project root')
    require(git(root, 'rev-parse', 'HEAD').decode().strip() == contract['consumer_source_commit'],
            'Current HEAD differs from continuation pin')
    require(git(root, 'status', '--porcelain') == b'', 'Source is dirty')
    for commit, expected in ((PRODUCER_COMMIT, PRODUCER_TREE),
                             (contract['consumer_source_commit'], contract['consumer_source_tree'])):
        require(git(root, 'rev-parse', commit + '^{tree}').decode().strip() == expected,
                'Source tree differs from pin')
    old = tree(root, PRODUCER_COMMIT)
    new = tree(root, contract['consumer_source_commit'])
    changes = {p: dict(before=old.get(p), after=new.get(p)) for p in sorted(set(old) | set(new))
               if old.get(p) != new.get(p)}
    require(same_json(changes, contract['source_changes']), 'Whole-tree diff exceeds exact reviewed changes')
    require(type(contract['source_files']) is dict and SOURCE_REQUIRED <= set(contract['source_files']),
            'Missing execution-source pins')
    for name, expected in contract['source_files'].items():
        safe_member(name)
        require(name in new and new[name]['sha256'] == expected and
                digest(root / name) == expected, 'Execution source changed: ' + name)
    structural = structural_module()
    require(structural.POLICY_SHA == STRUCTURAL_POLICY_SHA and
            structural.REUSE_POLICY_SHA == ACTUAL09_REUSE_POLICY_SHA, 'Structural/reuse implementation policy differs')
    keys(contract['external_source_files'], set(structural.EXTERNAL_PINS), 'External execution sources')
    for path, expected in contract['external_source_files'].items():
        require(digest(path) == hash_value(expected, 'External execution source hash'),
                'External execution source changed: ' + path)
    structural.tool_records(sys.modules[__name__], contract)
    return changes


def validate_inventory(value, expected_sha, expected_count=None):
    require(type(value) is dict and value, 'Missing proof inventory')
    total = 0
    for name, item in value.items():
        safe_member(name)
        keys(item, {'bytes', 'sha256'}, 'Inventory entry')
        total += number(item['bytes'], 'Member bytes')
        hash_value(item['sha256'], 'Member hash')
    require(len(value) < 1024 and total < 128 * 1024**2, 'Proof exceeds bounded inventory')
    if expected_count is not None:
        require(len(value) == expected_count, 'Wrong proof member count')
    require(sha_bytes(canonical(value)) == expected_sha, 'Proof inventory hash mismatch')
    return total


def inspect_proof(path, expected):
    """No extraction; bounded member reads and exact fixed terminal inventory."""
    record = file_record(path, PROOF_SHA, PROOF_BYTES)
    actual = {}
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        require(len(infos) == 300, 'Wrong terminal-proof member count')
        for info in infos:
            name = safe_member(info.filename)
            require(name not in actual and name in expected and not info.is_dir() and
                    not info.flag_bits & 1, 'Duplicate/unknown/encrypted proof member')
            member_mode = info.external_attr >> 16
            require(stat.S_IFMT(member_mode) in (0, stat.S_IFREG), 'Nonregular proof member')
            require(info.file_size == expected[name]['bytes'], 'Proof declared size mismatch')
            count, h = 0, hashlib.sha256()
            with archive.open(info) as stream:
                while True:
                    data = stream.read(min(1024 * 1024, info.file_size - count + 1))
                    if not data:
                        break
                    count += len(data)
                    require(count <= info.file_size, 'Proof member exceeds bound')
                    h.update(data)
            actual[name] = dict(bytes=count, sha256=h.hexdigest())
    require(actual == expected and unchanged([record]), 'Proof member bytes or file changed')
    return record


def local_producer_inventory(expected):
    found = {}
    for path in sorted(PRODUCER.rglob('*')):
        path_checked(path)
        require(not path.is_symlink(), 'Producer evidence symlink')
        if path.is_dir():
            continue
        if path in (PRODUCER / 'new-l3s1-producer.qcow2', PRODUCER / 'new-l3s1-immutable-backup.qcow2'):
            continue
        require(path.suffix.lower() not in ('.qcow2', '.iso'), 'Unexpected extra producer disk/ISO')
        regular(path)
        name = str(path.relative_to(PRODUCER))
        found[name] = dict(bytes=path.stat().st_size, sha256=digest(path))
    require(found == expected and len(found) == 139, 'Producer evidence inventory changed')
    return found


CONTRACT_KEYS = {'kind', 'design_sha256', 'structural_policy_sha256', 'archive_scope_addendum_sha256',
    'actual09_reuse_policy_sha256', 'archive_schema_sha256', 'root',
    'producer_source_commit', 'producer_source_tree', 'consumer_source_commit',
    'consumer_source_tree', 'source_changes', 'source_files', 'external_source_files', 'batch', 'paths'}
BATCH_KEYS = {'id', 'not_before_utc', 'stock_profile', 'frames', 'move_begin',
              'move_end_exclusive', 'fire_at'}
CONSUMER_PATH_KEYS = {'parent_prefix', 'build_manifest', 'build_start', 'build_log', 'consumer_out',
                      'durable_backup_receipt'}
APPROVAL_KEYS = {'kind', 'approved', 'user_approval_message_id', 'design_sha256',
    'archive_scope_addendum_sha256', 'structural_policy_sha256', 'workspace_build_manifest_sha256',
    'actual09_reuse_policy_sha256',
    'source_manifest_path', 'source_manifest_sha256', 'continuation_contract_sha256',
    'repository_head', 'allowed_repository_deltas', 'paths', 'archive_max_bytes',
    'transport_temporary_bytes', 'transport_bound_verified_by_parent',
    'transport_bound_evidence_sha256', 'reserve_bytes', 'small_overhead_bytes'}
ARCHIVE_PATH_KEYS = {'staging_dir', 'archive_readback', 'archive_materialization_record',
    'archive_upload_results', 'reconstruction', 'reconstruction_record', 'receipt',
    'parent_verification', 'terminal_proof_create_record', 'terminal_proof_readback_record',
    'archive_materialization_raw_evidence', 'terminal_proof_materialization_raw_evidence'}
RECEIPT_KEYS = {'kind', 'representation', 'status', 'remote_upload_verified_by_parent',
    'producer_hdd_sha256', 'producer_hdd_bytes', 'producer_manifest_sha256',
    'producer_verification_sha256', 'producer_build_manifest_sha256',
    'producer_source_commit', 'producer_xbe_sha256', 'producer_map_sha256',
    'producer_evidence_inventory_sha256', 'artifacts', 'raw_reconstruction',
    'archive_inner_manifest_sha256', 'proof_member_inventory', 'proof_member_inventory_sha256',
    'continuation_contract_sha256', 'source_manifest_sha256', 'approval_sha256',
    'evidence', 'verification_scope'}
EVIDENCE_KEYS = {'staging_sha256', 'archive_upload_results_sha256',
    'archive_materialization_record_sha256', 'terminal_proof_create_record_sha256',
    'terminal_proof_readback_record_sha256', 'reconstruction_record_sha256',
    'parent_verification_sha256'}
STAGING_KEYS = {'kind', 'producer', 'proof', 'archive', 'inner_manifest', 'approval_sha256',
    'source_manifest_sha256', 'continuation_contract_sha256', 'archive_max_bytes',
    'transport_temporary_bytes', 'verification_scope'}
PRODUCER_STAGE_KEYS = {'path', 'backup', 'raw_staging_path', 'hdd_sha256', 'hdd_bytes',
    'manifest_sha256', 'verification_sha256', 'build_manifest_sha256', 'source_commit',
    'xbe_sha256', 'map_sha256', 'evidence_inventory_sha256'}
MATERIALIZATION_KEYS = {'local_path', 'library_file_id', 'version', 'file_id', 'file_name',
    'sha256', 'bytes', 'device', 'inode', 'materialized_utc', 'scope'}
PARENT_KEYS = {'kind', 'verified_by_parent', 'approval_sha256', 'source_manifest_sha256',
    'continuation_contract_sha256', 'staging_sha256', 'archive_upload_results_sha256',
    'archive_materialization_record_sha256', 'terminal_proof_create_record_sha256',
    'terminal_proof_readback_record_sha256', 'archive_materialization_raw_evidence_sha256',
    'terminal_proof_materialization_raw_evidence_sha256'}
RECONSTRUCTION_KEYS = {'kind', 'approval_sha256', 'source_manifest_sha256', 'staging_sha256',
    'archive_upload_results_sha256', 'archive_materialization_record_sha256',
    'archive_inner_manifest_sha256', 'proof_member_inventory_sha256', 'raw',
    'source_archive', 'verification_scope'}


def outside_protected(value):
    path = path_checked(value, exists=False)
    for forbidden in (ROOT, PRODUCER, RAW_STAGING.parent):
        require(path != forbidden and forbidden not in path.parents,
                'Continuation target inside protected directory')
    require('20261011-0800-parent' not in path.name and
            path != Path('/workspace/shared/rf-natural-save-durable-backup-receipt-20261011-0800.json') and
            path != Path('/workspace/shared/rf-natural-save-durable-backup-20261011-0800.failed'),
            'Old terminal target cannot be reused')
    return path


def require_hourly_ownership(contract):
    """Base-hour aliases are a consumed/queued batch, even under another prefix."""
    batch = contract['batch']['id']
    require(batch.endswith('-saved-consumer'), 'Invalid continuation batch suffix')
    slot = batch[:-len('-saved-consumer')]
    prefix = Path('/workspace/shared/rf-' + slot + '-parent')
    forbidden = [Path(str(prefix) + suffix) for suffix in ('.queued', '.started', '.log', '.exit.json')]
    forbidden.extend((Path('/workspace/shared/rf-' + slot + '-build.json'),
                      Path('/workspace/shared/rf-' + slot + '-build-start.json'),
                      ROOT / 'artifacts/hourly' / (slot + '-build.log')))
    # Own queued/started/log markers belong to the live launcher and are expected.
    # Its terminal marker, by contrast, permanently closes this attempt.
    forbidden.append(Path(contract['paths']['parent_prefix'] + '.exit.json'))
    for path in forbidden:
        require(not os.path.lexists(path), 'Hourly slot or own attempt is already queued/consumed: ' + str(path))


def validate_contract(value):
    keys(value, CONTRACT_KEYS, 'Continuation contract')
    require(value['kind'] == 'RF_NATURAL_SAVE_CONSUMER_CONTINUATION_CONTRACT_V3' and
            value['design_sha256'] == DESIGN_SHA and value['structural_policy_sha256'] == STRUCTURAL_POLICY_SHA,
            'Wrong reviewed baseline/structural continuation design')
    require(value['actual09_reuse_policy_sha256'] == ACTUAL09_REUSE_POLICY_SHA, 'Wrong explicit exact09 reuse design')
    require(value['archive_scope_addendum_sha256'] == ARCHIVE_SCOPE_SHA,
            'Wrong explicit structural-consumer archive scope')
    require(value['root'] == str(ROOT) and value['producer_source_commit'] == PRODUCER_COMMIT and
            value['producer_source_tree'] == PRODUCER_TREE, 'Wrong producer/root contract')
    hash_value(value['consumer_source_commit'], 'Consumer source', 40)
    hash_value(value['consumer_source_tree'], 'Consumer tree', 40)
    hash_value(value['archive_schema_sha256'], 'Archive schema hash')
    require(type(value['source_changes']) is dict and value['source_changes'],
            'Missing exact reviewed source changes')
    for name, change in value['source_changes'].items():
        safe_member(name)
        keys(change, {'before', 'after'}, 'Source delta')
        validate_tree_entry(change['before'])
        validate_tree_entry(change['after'])
        require(change['before'] != change['after'], 'Empty source delta')
        for entry in (change['before'], change['after']):
            require(entry is None or entry['mode'] == '100644', 'Changed source must be regular mode100644')
        if change['before'] and change['after']:
            require(change['before']['sha256'] != change['after']['sha256'],
                    'Mode/type-only source changes are not admitted')
    batch = keys(value['batch'], BATCH_KEYS, 'Continuation batch')
    require(type(batch['id']) is str and re.fullmatch(r'\d{8}-\d{4}-saved-consumer', batch['id']),
            'Unfinalized/invalid batch identity')
    require(batch['id'] not in ('20261011-0800-saved-consumer', '20261011-0900-saved-consumer'),
            'Known consumed08/09 hourly slots cannot run another build')
    require(type(batch['not_before_utc']) is str and batch['not_before_utc'].endswith('+00:00'),
            'Require exact UTC hourly not-before')
    moment = datetime.datetime.fromisoformat(batch['not_before_utc'])
    require(moment > datetime.datetime(2026, 10, 11, 9, tzinfo=datetime.timezone.utc),
            'Continuation must use a later hourly slot than completed09')
    require(moment.minute == moment.second == moment.microsecond == 0 and
            moment.strftime('%Y%m%d-%H%M') + '-saved-consumer' == batch['id'],
            'Batch identity is not its hourly slot')
    for name, expected in (('stock_profile', 0), ('frames', 240), ('move_begin', 120),
                           ('move_end_exclusive', 135), ('fire_at', 180)):
        require(type(batch[name]) is int and batch[name] == expected, 'Changed native consumer schedule')
    paths = keys(value['paths'], CONSUMER_PATH_KEYS, 'Continuation paths')
    require(len(set(paths.values())) == len(paths), 'Continuation paths overlap')
    for name, path in paths.items():
        if name == 'build_log':
            actual = path_checked(path, exists=False)
            require(actual.parent == ROOT / 'artifacts/hourly' and
                    actual.name == batch['id'] + '-build.log', 'Wrong build log target')
        else:
            actual = outside_protected(path)
            require(batch['id'] in str(actual), 'Continuation target lacks separate batch identity')
    require_hourly_ownership(value)
    return value


def load_context(root, contract_path, contract_sha, approval_path, approval_sha,
                 receipt_path, receipt_sha):
    """Authenticate exact locally pinned records; service/user authority is parent-owned."""
    require(Path(root) == ROOT, 'Wrong root')
    contract, contract_bytes = read_json(contract_path, contract_sha)
    validate_contract(contract)
    changes = validate_sources(contract)
    approval, approval_bytes = read_json(approval_path, approval_sha)
    keys(approval, APPROVAL_KEYS, 'Archive execution approval')
    require(approval['kind'] == 'RF_EXACT_ARCHIVE_PARENT_APPROVAL_EXACT09_REUSE_V4' and
            approval['approved'] is True and approval['design_sha256'] == ARCHIVE_DESIGN_SHA and
            type(approval['user_approval_message_id']) is str and approval['user_approval_message_id'],
            'Missing exact parent-supplied archive authorization')
    require(approval['archive_scope_addendum_sha256'] == contract['archive_scope_addendum_sha256'] == ARCHIVE_SCOPE_SHA and
            approval['structural_policy_sha256'] == contract['structural_policy_sha256'] == STRUCTURAL_POLICY_SHA and
            approval['actual09_reuse_policy_sha256'] == contract['actual09_reuse_policy_sha256'] == ACTUAL09_REUSE_POLICY_SHA and
            approval['workspace_build_manifest_sha256'] == WORKSPACE_BUILD_SHA,
            'Archive approval does not authorize this explicit successor scope')
    historical, historical_bytes = read_json(WORKSPACE_BUILD_PATH, WORKSPACE_BUILD_SHA)
    keys(historical, {'status', 'source_commit', 'xbe', 'map', 'runtime'}, 'Historical actual09 build')
    require(same_json(historical, dict(status='PASS_XBOX_BUILD',
            source_commit='3ed8c4a07a6563707803d6b49531947d2a525b4f',
            xbe='32fcd13eb5765831d9b407deffbe86bc6044aeb41d1cf34000e72dc168c18bc0', map=MAP_SHA,
            runtime='NOT_RUN_BUILD_ONLY_FALLBACK_BACKUP_PERMISSION_OR_CONSUMER_ADMISSION_PENDING')),
            'Historical archive-time workspace build differs')
    require(approval['continuation_contract_sha256'] == contract_sha and
            approval['repository_head'] == contract['consumer_source_commit'],
            'Archive approval belongs to another continuation')
    projected = {name: dict(before_sha256=entry['before']['sha256'] if entry['before'] else None,
                           after_sha256=entry['after']['sha256'] if entry['after'] else None)
                 for name, entry in changes.items()}
    require(same_json(approval['allowed_repository_deltas'], projected), 'Archive source exceptions differ')
    paths = keys(approval['paths'], ARCHIVE_PATH_KEYS, 'Archive paths')
    require(len(set(paths.values())) == len(paths), 'Archive paths overlap')
    for name, value in paths.items():
        path_checked(value)
        if name not in ('terminal_proof_create_record', 'terminal_proof_readback_record',
                        'archive_materialization_raw_evidence', 'terminal_proof_materialization_raw_evidence'):
            outside_protected(value)
    require(approval['transport_bound_verified_by_parent'] is True and
            approval['reserve_bytes'] == RESERVE and type(approval['reserve_bytes']) is int and
            approval['small_overhead_bytes'] == 64 * 1024**2 and type(approval['small_overhead_bytes']) is int,
            'Storage bound not approved')
    number(approval['transport_temporary_bytes'], 'Transport temporary bound')
    number(approval['archive_max_bytes'], 'Archive cap', True)
    require(approval['archive_max_bytes'] < 2**31, 'Archive cap outside reviewed representation')
    hash_value(approval['transport_bound_evidence_sha256'], 'Transport-bound evidence')
    source, source_bytes = read_json(approval['source_manifest_path'], approval['source_manifest_sha256'])
    keys(source, {'kind', 'files'}, 'Archive source manifest')
    require(source['kind'] == 'RF_EXACT_ARCHIVE_SOURCE_MANIFEST_V2', 'Wrong archive source manifest')
    keys(source['files'], {'archive_v2_common.py', 'prepare_exact_archive_v2.py',
                          'verify_publish_exact_archive_v2.py', 'SCHEMA-CONTRACT.md'}, 'Archive sources')
    for name, expected in source['files'].items():
        require(digest(Path(approval['source_manifest_path']).parent / name) ==
                hash_value(expected, 'Archive source hash'), 'Archive source changed: ' + name)
    require(source['files']['SCHEMA-CONTRACT.md'] == contract['archive_schema_sha256'],
            'Shared V2 schema differs from exact reviewed contract')
    require(str(receipt_path) == paths['receipt'] == contract['paths']['durable_backup_receipt'],
            'Wrong separately identified receipt path')
    receipt, receipt_bytes = read_json(receipt_path, receipt_sha)
    keys(receipt, RECEIPT_KEYS, 'V2 durable receipt')
    require(receipt['kind'] == 'RF_NATURAL_SAVE_DURABLE_BACKUP_V2' and
            receipt['representation'] == REPRESENTATION and
            receipt['status'] == 'PASS_EXACT_ARCHIVE_BACKUP_AND_RAW_RECONSTRUCTION' and
            receipt['remote_upload_verified_by_parent'] is True,
            'Missing fully verified exact-archive V2 receipt')
    for name, expected in dict(producer_hdd_sha256=HDD_SHA, producer_hdd_bytes=HDD_BYTES,
            producer_manifest_sha256=MANIFEST_SHA, producer_verification_sha256=VERIFICATION_SHA,
            producer_build_manifest_sha256=BUILD_SHA, producer_source_commit=PRODUCER_COMMIT,
            producer_xbe_sha256=XBE_SHA, producer_map_sha256=MAP_SHA,
            continuation_contract_sha256=contract_sha, approval_sha256=approval_sha,
            source_manifest_sha256=approval['source_manifest_sha256'],
            proof_member_inventory_sha256=PROOF_INVENTORY_SHA).items():
        require(type(receipt[name]) is type(expected) and receipt[name] == expected,
                'Receipt identity differs: ' + name)
    for name in ('producer_evidence_inventory_sha256', 'archive_inner_manifest_sha256'):
        hash_value(receipt[name], name)
    require(type(receipt['verification_scope']) is str and receipt['verification_scope'],
            'Missing receipt scope')
    keys(receipt['evidence'], EVIDENCE_KEYS, 'Receipt evidence')
    for value in receipt['evidence'].values():
        hash_value(value, 'Evidence hash')
    require(receipt['evidence']['terminal_proof_create_record_sha256'] == PROOF_CREATE_SHA and
            receipt['evidence']['terminal_proof_readback_record_sha256'] == PROOF_READBACK_RECORD_SHA,
            'Confirmed terminal-proof records changed')
    records = [file_record(path, expected) for path, expected in
        ((contract_path, contract_sha), (approval_path, approval_sha), (receipt_path, receipt_sha),
         (approval['source_manifest_path'], approval['source_manifest_sha256']),
         (WORKSPACE_BUILD_PATH, WORKSPACE_BUILD_SHA))]
    records.extend(file_record(Path(approval['source_manifest_path']).parent / name, expected)
                   for name, expected in source['files'].items())
    records.extend(file_record(ROOT / name, expected) for name, expected in contract['source_files'].items())
    records.extend(file_record(path, expected) for path, expected in contract['external_source_files'].items())
    return dict(contract=contract, contract_bytes=contract_bytes, contract_sha256=contract_sha,
        approval=approval, approval_sha256=approval_sha, receipt=receipt, receipt_bytes=receipt_bytes,
        receipt_sha256=receipt_sha, protected_records=records, admission_complete=False)


def artifact(value, role):
    keys(value, ARTIFACT_KEYS, 'Durable artifact')
    require(value['role'] == role and type(value['library_file_id']) is str and
            value['library_file_id'].startswith('libfile_') and type(value['file_id']) is str and
            value['file_id'].startswith('file_') and type(value['file_name']) is str and
            value['file_name'] and '/' not in value['file_name'] and '\\' not in value['file_name'],
            'Invalid durable artifact identity')
    number(value['version'], 'Library version')
    number(value['bytes'], 'Artifact bytes', True)
    hash_value(value['sha256'], 'Artifact hash')
    path_checked(value['local_path'])
    path_checked(value['readback_path'])
    return value


def match_file_record(record, current, readonly=False):
    keys(record, RAW_KEYS if readonly else FILE_KEYS, 'Recorded file identity')
    for name in FILE_KEYS:
        require(type(record[name]) is type(current[name]) and record[name] == current[name],
                'File-record join differs: ' + name)
    if readonly:
        require(record['read_only'] is True and current['mode'] & 0o222 == 0, 'Seed is not read-only')


def compare_materialization(record, artifact_value, file_value):
    keys(record, MATERIALIZATION_KEYS, 'Materialization record')
    for name in ('library_file_id', 'version', 'file_id', 'file_name', 'sha256', 'bytes'):
        require(type(record[name]) is type(artifact_value[name]) and record[name] == artifact_value[name],
                'Materialization identity differs: ' + name)
    require(record['local_path'] == file_value['path'] == artifact_value['readback_path'],
            'Materialization path differs')
    for name in ('device', 'inode'):
        require(type(record[name]) is int and record[name] == file_value[name], 'Readback inode changed')
    require(type(record['materialized_utc']) is str and type(record['scope']) is str and record['scope'],
            'Missing actual materialization scope/time')
    moment = datetime.datetime.fromisoformat(record['materialized_utc'])
    require(moment.utcoffset() == datetime.timedelta(0), 'Non-UTC materialization time')
    require(os.getxattr(file_value['path'], 'user.library-file-version') ==
            str(artifact_value['version']).encode('ascii'), 'Library readback version metadata differs')


def read_evidence(ctx, name, path):
    expected = ctx['receipt']['evidence'][name + '_sha256']
    value, data = read_json(path, expected)
    ctx['protected_records'].append(file_record(path, expected))
    return value, data


def precheck_producer(ctx, folder):
    """Bound legacy reader inputs before it opens any protected HDD."""
    require(path_checked(folder) == PRODUCER, 'Wrong continuation producer path')
    require(ctx['contract']['producer_source_commit'] == PRODUCER_COMMIT, 'Wrong original source')
    require(not os.path.lexists(PRODUCER / 'consumer-attempt.json'), 'Consumer attempt already reserved')
    for name, expected in (('producer-immutable.json', MANIFEST_SHA),
                           ('verification.json', VERIFICATION_SHA), ('consumer-build.json', BUILD_SHA)):
        read_json(PRODUCER / name, expected)
    records = []
    for name in ('new-l3s1-producer.qcow2', 'new-l3s1-immutable-backup.qcow2'):
        path = PRODUCER / name
        info = regular(path, readonly=True)
        require(info.st_size == HDD_BYTES, 'Protected HDD declared size differs')
        records.append((str(path), info.st_dev, info.st_ino))
    require(len({(device, inode) for unused, device, inode in records}) == 2,
            'Protected producer/backup share inode')
    original_build, unused = read_json(PRODUCER / 'consumer-build.json', BUILD_SHA)
    return original_build


def proof_service_joins(raw, proof_create, proof):
    keys(raw, {'proof_create_structured_result', 'proof_prepare_materialize_tool_result'},
         'Retained terminal-proof service evidence')
    require(same_json(raw['proof_create_structured_result'],
                     {key: value for key, value in proof_create.items() if key != 'archive_sha256'}),
            'Retained actual proof creation differs')
    result = raw['proof_prepare_materialize_tool_result']
    require(type(result) is dict and result.get('isError') is False, 'Proof materialization tool failed')
    structured = result.get('structuredContent')
    require(type(structured) is dict and structured.get('unavailable_items') == [] and
            type(structured.get('transfers')) is list and len(structured['transfers']) == 1,
            'Proof materialization transfer incomplete')
    transfer = structured['transfers'][0]
    require(type(transfer) is dict, 'Malformed proof transfer')
    for name in ('library_file_id', 'file_id', 'file_name'):
        require(transfer.get(name) == proof[name], 'Actual proof transfer identity differs')
    require(type(transfer.get('current_version_number')) is int and transfer['current_version_number'] == 0 and
            type(transfer.get('size_bytes')) is int and transfer['size_bytes'] == PROOF_BYTES and
            transfer.get('workspace_path') == proof['readback_path'] and
            same_json(transfer.get('xattrs'), [{'name': 'user.library-file-version', 'value': '0'}]),
            'Actual proof transfer version/path/size differs')


def join_producer_fields(receipt):
    return dict(path=str(PRODUCER / 'new-l3s1-producer.qcow2'),
        backup=str(PRODUCER / 'new-l3s1-immutable-backup.qcow2'),
        raw_staging_path=str(RAW_STAGING), hdd_sha256=HDD_SHA, hdd_bytes=HDD_BYTES,
        manifest_sha256=MANIFEST_SHA, verification_sha256=VERIFICATION_SHA,
        build_manifest_sha256=BUILD_SHA, source_commit=PRODUCER_COMMIT,
        xbe_sha256=XBE_SHA, map_sha256=MAP_SHA,
        evidence_inventory_sha256=receipt['producer_evidence_inventory_sha256'])


def require_space(ctx, destination, consumer_copy_needed):
    destination = path_checked(destination)
    info = destination.stat()
    require(info.st_dev == PRODUCER.stat().st_dev, 'Consumer filesystem differs from capacity contract')
    usage = os.statvfs(destination)
    required = RESERVE + ctx['approval']['small_overhead_bytes'] + ctx['approval']['transport_temporary_bytes']
    if consumer_copy_needed:
        required += HDD_BYTES
    require(usage.f_bavail * usage.f_frsize >= required, 'Consumer copy/reserve capacity unavailable')


def admit(ctx, admitted, report, build_path, consumer_out):
    """Exact admission before media mutation; retain only new comparison evidence."""
    contract, approval, receipt = ctx['contract'], ctx['approval'], ctx['receipt']
    paths = approval['paths']
    require(Path(admitted['folder']) == PRODUCER, 'Wrong producer folder')
    require(str(build_path) == contract['paths']['build_manifest'] and
            str(consumer_out) == contract['paths']['consumer_out'], 'Wrong continuation build/output')
    require(not os.path.lexists(PRODUCER / 'consumer-attempt.json'), 'Producer consumer attempt already reserved')
    validate_sources(contract)
    build, build_bytes = read_json(build_path)
    keys(build, structural_module().BUILD_KEYS, 'Actual consumer build')
    require(build['status'] == 'PASS_XBOX_BUILD' and build['source_commit'] == contract['consumer_source_commit'] and
            build['source_tree'] == contract['consumer_source_tree'] and build['batch'] == contract['batch']['id'] and
            type(build['stock_profile']) is int and build['stock_profile'] == 0 and
            build['runtime'] == 'PENDING_VERIFIED_SAVED_CONSUMER_ONLY' and
            build['map'] == MAP_SHA, 'Incompatible actual consumer build')
    hash_value(build['xbe'], 'Actual consumer raw XBE')
    number(build['parent_runner_pid'], 'Actual parent PID', True)
    number(build['parent_runner_start_ticks'], 'Actual parent start', True)
    require(build['parent_runner_pid'] == os.getppid(), 'Build manifest belongs to another live parent')
    process_stat = Path('/proc/' + str(os.getppid()) + '/stat').read_text()
    require(int(process_stat.rsplit(')', 1)[1].split()[19]) == build['parent_runner_start_ticks'],
            'Parent process identity was reused')
    require(build == report['consumer_build'] and sha_bytes(build_bytes) == report['consumer_build_sha256'],
            'Actual consumer build bytes changed')
    require(report['source_commit'] == contract['consumer_source_commit'] and report['phase'] == 'consumer',
            'Continuation is not this consumer source/phase')
    ctx['protected_records'].append(file_record(build_path, sha_bytes(build_bytes)))
    ctx['build_sha256'] = sha_bytes(build_bytes)
    ctx['consumer_out'] = str(consumer_out)
    structural_module().qualify(sys.modules[__name__], ctx, build, build_bytes)
    inventory = receipt['proof_member_inventory']
    require(validate_inventory(inventory, PROOF_INVENTORY_SHA, 300) == 20725366 and
            len(canonical(inventory)) == 46333, 'Wrong exact terminal-proof inventory')
    producer_inventory = {name[len('producer/'):]: item for name, item in inventory.items()
                          if name.startswith('producer/')}
    validate_inventory(producer_inventory, receipt['producer_evidence_inventory_sha256'], 139)
    local_producer_inventory(producer_inventory)
    verification, verification_bytes = read_json(PRODUCER / 'verification.json', VERIFICATION_SHA)
    manifest, manifest_bytes = read_json(PRODUCER / 'producer-immutable.json', MANIFEST_SHA)
    original_build, original_build_bytes = read_json(PRODUCER / 'consumer-build.json', BUILD_SHA)
    require(same_json(admitted['verification'], verification) and same_json(admitted['manifest'], manifest) and
            same_json(original_build, verification['consumer_build']), 'Original evidence join differs')
    require(original_build['source_commit'] == PRODUCER_COMMIT and original_build['source_tree'] == PRODUCER_TREE and
            original_build['status'] == 'PASS_XBOX_BUILD' and original_build['xbe'] == XBE_SHA and
            original_build['map'] == MAP_SHA, 'Original build identity changed')
    require(verification['base_hdd_sha256'] == report['base_hdd_sha256'] and
            verification['original_archive_sha256'] == report['original_archive_sha256'] and
            verification['fixed_data_sha256'] == report['fixed_data_sha256'] == FIXED and
            verification['recipe'] == report['recipe'] and
            verification['symbol_addresses'] == report['symbol_addresses'], 'Producer/consumer game inputs differ')
    expected_names = set(verification['original_archive_sha256']) | set(FIXED) | {'default.xbe'}
    disc = ROOT / 'build/xbox/disc'
    require(len(expected_names) == 20 and sorted(expected_names) == verification['original_disc_names'] ==
            report['original_disc_names'] == sorted(path.name for path in disc.iterdir()),
            'Complete original media inventory differs')
    for path in disc.iterdir():
        regular(path)
    require(digest(disc / 'default.xbe') == ctx['consumer_xbe_sha256'] == build['xbe'] and
            digest(ROOT / 'build/xbox/main.map') == MAP_SHA, 'Qualified raw future artifacts changed')
    artifacts = receipt['artifacts']
    require(type(artifacts) is list and len(artifacts) == 2, 'Require exactly two logical artifacts')
    archive = artifact(artifacts[0], 'hdd_archive')
    proof = artifact(artifacts[1], 'terminal_proof')
    for name, expected in PROOF_IDENTITY.items():
        require(type(proof[name]) is type(expected) and proof[name] == expected, 'Wrong terminal-proof identity')
    require(proof['local_path'] == str(PROOF_LOCAL) and proof['readback_path'] == str(PROOF_READBACK),
            'Wrong confirmed proof paths')
    require(archive['library_file_id'] != proof['library_file_id'] and archive['file_id'] != proof['file_id'],
            'HDD archive and terminal proof share remote identity')
    expected_archive = Path(paths['staging_dir']) / 'OpenRedFaction-New-L3S1-Exact-HDD-Archive-20261011-0800.zip'
    require(archive['local_path'] == str(expected_archive) and archive['readback_path'] == paths['archive_readback'],
            'Archive paths differ from approval')
    require(archive['bytes'] <= approval['archive_max_bytes'], 'Archive exceeded approved cap')
    stage, stage_bytes = read_evidence(ctx, 'staging', Path(paths['staging_dir']) / 'archive-staging.json')
    keys(stage, STAGING_KEYS, 'Archive staging')
    require(stage['kind'] == 'RF_NATURAL_SAVE_ARCHIVE_STAGING_V2', 'Wrong staging kind')
    keys(stage['producer'], PRODUCER_STAGE_KEYS, 'Staged producer')
    require(same_json(stage['producer'], join_producer_fields(receipt)), 'Staged original producer differs')
    keys(stage['archive'], {'path', 'sha256', 'bytes', 'inner_manifest_sha256'}, 'Staged archive')
    require(same_json(stage['archive'], dict(path=archive['local_path'], sha256=archive['sha256'], bytes=archive['bytes'],
            inner_manifest_sha256=receipt['archive_inner_manifest_sha256'])), 'Staged archive differs')
    keys(stage['proof'], {'artifact', 'member_inventory', 'member_inventory_sha256',
        'producer_evidence_inventory_sha256', 'create_record_sha256', 'readback_record_sha256'}, 'Staged proof')
    require(same_json(stage['proof'], dict(artifact=proof, member_inventory=inventory,
            member_inventory_sha256=PROOF_INVENTORY_SHA,
            producer_evidence_inventory_sha256=receipt['producer_evidence_inventory_sha256'],
            create_record_sha256=receipt['evidence']['terminal_proof_create_record_sha256'],
            readback_record_sha256=receipt['evidence']['terminal_proof_readback_record_sha256'])), 'Staged proof differs')
    for name in ('approval_sha256', 'source_manifest_sha256', 'continuation_contract_sha256'):
        require(stage[name] == receipt[name], 'Staging authorization differs')
    require(type(stage['archive_max_bytes']) is int and stage['archive_max_bytes'] == approval['archive_max_bytes'] and
            type(stage['transport_temporary_bytes']) is int and
            stage['transport_temporary_bytes'] == approval['transport_temporary_bytes'] and
            type(stage['verification_scope']) is str and stage['verification_scope'], 'Staging bounds/scope differ')
    proof_identity = {name: value for name, value in proof.items() if name not in ('local_path', 'readback_path')}
    expected_inner = dict(kind='RF_NATURAL_SAVE_EXACT_QCOW2_ARCHIVE_V1', purpose='NEW_L3S1_NATURAL_SAVE',
        representation=REPRESENTATION, member='new-l3s1-producer.qcow2', uncompressed_sha256=HDD_SHA,
        uncompressed_bytes=HDD_BYTES, producer_manifest_sha256=MANIFEST_SHA,
        producer_verification_sha256=VERIFICATION_SHA, producer_build_manifest_sha256=BUILD_SHA,
        producer_source_commit=PRODUCER_COMMIT, producer_xbe_sha256=XBE_SHA, producer_map_sha256=MAP_SHA,
        producer_evidence_inventory_sha256=receipt['producer_evidence_inventory_sha256'],
        proof_artifact=proof_identity, proof_member_inventory=inventory,
        proof_member_inventory_sha256=PROOF_INVENTORY_SHA, continuation_contract_sha256=ctx['contract_sha256'],
        approval_sha256=ctx['approval_sha256'])
    require(same_json(stage['inner_manifest'], expected_inner) and len(canonical(expected_inner)) <= 65536 and
            sha_bytes(canonical(expected_inner)) == receipt['archive_inner_manifest_sha256'], 'Inner manifest differs')
    archive_local = file_record(archive['local_path'], archive['sha256'], archive['bytes'])
    archive_readback = file_record(archive['readback_path'], archive['sha256'], archive['bytes'])
    # Publication already decoded the pinned remote archive. Admission rechecks
    # the same outer bytes and exact manifest, then fully reads its saved raw
    # reconstruction; it does not reconstruct another HDD or trust a local seed.
    with zipfile.ZipFile(archive['readback_path']) as zipped:
        members = zipped.infolist()
        require([item.filename for item in members] == ['manifest.json', 'new-l3s1-producer.qcow2'],
                'Unexpected archive members')
        require(all(item.compress_type == zipfile.ZIP_DEFLATED and not item.flag_bits & 9 for item in members) and
                members[0].file_size == len(canonical(expected_inner)) and members[1].file_size == HDD_BYTES,
                'Archive header policy differs')
        require(zipped.read(members[0]) == canonical(expected_inner), 'Actual archive manifest differs')
    proof_local = inspect_proof(proof['local_path'], inventory)
    proof_readback = inspect_proof(proof['readback_path'], inventory)
    upload, unused_upload_bytes = read_evidence(ctx, 'archive_upload_results', paths['archive_upload_results'])
    require(type(upload) is dict and type(upload.get('results')) is list and len(upload['results']) == 1,
            'Require exactly one actual archive upload result')
    result = upload['results'][0]
    require(type(result) is dict and result.get('status') == 'succeeded' and
            result.get('purpose') == 'create_library_file' and result.get('local_path') == archive['local_path'],
            'Archive upload did not succeed for exact staged file')
    for name in ('library_file_id', 'file_id', 'file_name'):
        require(result.get(name) == archive[name], 'Upload artifact join differs')
    require(type(result.get('current_version_number')) is int and result['current_version_number'] == archive['version'],
            'Upload version differs')
    archive_materialization, unused_am = read_evidence(ctx, 'archive_materialization_record', paths['archive_materialization_record'])
    compare_materialization(archive_materialization, archive, archive_readback)
    proof_materialization, unused_pm = read_evidence(ctx, 'terminal_proof_readback_record', paths['terminal_proof_readback_record'])
    compare_materialization(proof_materialization, proof, proof_readback)
    proof_create, unused_pc = read_evidence(ctx, 'terminal_proof_create_record', paths['terminal_proof_create_record'])
    keys(proof_create, {'operation', 'status', 'path', 'file_id', 'library_file_id', 'file_name',
        'current_version_number', 'restored_from_version_number', 'xattrs', 'directory_id', 'mime_type',
        'library_artifact_type', 'file_size_bytes', 'external_connectors_accessed', 'archive_sha256'},
        'Confirmed terminal-proof creation record')
    require(type(proof_create) is dict and proof_create.get('operation') == 'create_library_file' and
            proof_create.get('status') == 'succeeded', 'Missing actual proof creation')
    for name in ('library_file_id', 'file_id', 'file_name'):
        require(proof_create.get(name) == proof[name], 'Proof creation identity differs')
    require(type(proof_create.get('current_version_number')) is int and proof_create['current_version_number'] == 0 and
            type(proof_create.get('file_size_bytes')) is int and proof_create['file_size_bytes'] == PROOF_BYTES and
            proof_create.get('archive_sha256') == PROOF_SHA and
            proof_create.get('path') == '/' + PROOF_IDENTITY['file_name'] and
            proof_create.get('restored_from_version_number') is None and
            same_json(proof_create.get('xattrs'), [{'name': 'user.library-file-version', 'value': '0'}]),
            'Proof creation byte/version join differs')
    parent, unused_parent = read_evidence(ctx, 'parent_verification', paths['parent_verification'])
    keys(parent, PARENT_KEYS, 'Parent verification')
    require(parent['kind'] == 'RF_EXACT_ARCHIVE_REMOTE_VERIFICATION_V2' and parent['verified_by_parent'] is True,
            'Missing authenticated parent remote verification')
    for name in ('approval_sha256', 'source_manifest_sha256', 'continuation_contract_sha256'):
        require(parent[name] == receipt[name], 'Parent verification authority differs')
    for name in ('staging_sha256', 'archive_upload_results_sha256', 'archive_materialization_record_sha256',
                 'terminal_proof_create_record_sha256', 'terminal_proof_readback_record_sha256'):
        require(parent[name] == receipt['evidence'][name], 'Parent evidence join differs')
    for name in ('archive_materialization_raw_evidence', 'terminal_proof_materialization_raw_evidence'):
        expected = hash_value(parent[name + '_sha256'], 'Actual retained transfer evidence hash')
        ctx['protected_records'].append(file_record(paths[name], expected))
        if name == 'terminal_proof_materialization_raw_evidence':
            require(expected == PROOF_RAW_SERVICE_SHA, 'Confirmed proof raw service record changed')
            raw_service, unused_service = read_json(paths[name], expected)
            proof_service_joins(raw_service, proof_create, proof)
    reconstruction, unused_reconstruction = read_evidence(ctx, 'reconstruction_record', paths['reconstruction_record'])
    keys(reconstruction, RECONSTRUCTION_KEYS, 'Raw reconstruction')
    require(reconstruction['kind'] == 'RF_NATURAL_SAVE_RAW_RECONSTRUCTION_V2' and
            type(reconstruction['verification_scope']) is str and reconstruction['verification_scope'],
            'Missing complete raw-reconstruction record')
    for name in ('approval_sha256', 'source_manifest_sha256', 'archive_inner_manifest_sha256', 'proof_member_inventory_sha256'):
        require(reconstruction[name] == receipt[name], 'Reconstruction authority differs')
    for name in ('staging_sha256', 'archive_upload_results_sha256', 'archive_materialization_record_sha256'):
        require(reconstruction[name] == receipt['evidence'][name], 'Reconstruction evidence differs')
    match_file_record(reconstruction['source_archive'], archive_readback)
    raw = keys(receipt['raw_reconstruction'], RAW_KEYS, 'Reconstructed raw seed')
    require(raw['path'] == paths['reconstruction'] and same_json(reconstruction['raw'], raw),
            'Reconstructed seed path/record differs')
    raw_record = file_record(raw['path'], HDD_SHA, HDD_BYTES, readonly=True)
    match_file_record(raw, raw_record, readonly=True)
    originals = [file_record(PRODUCER / name, HDD_SHA, HDD_BYTES, readonly=True) for name in
                 ('new-l3s1-producer.qcow2', 'new-l3s1-immutable-backup.qcow2')]
    staging_record = file_record(RAW_STAGING, HDD_SHA, HDD_BYTES)
    disk_and_artifacts = originals + [staging_record, raw_record, archive_local, archive_readback, proof_local, proof_readback]
    independent(disk_and_artifacts)
    ctx['protected_records'].extend(disk_and_artifacts)
    ctx['protected_records'].extend(file_record(PRODUCER / name, item['sha256'], item['bytes'])
                                   for name, item in producer_inventory.items())
    require_space(ctx, consumer_out, consumer_copy_needed=True)
    require(not os.path.lexists(PRODUCER / 'consumer-attempt.json'), 'Consumer reservation appeared during admission')
    ctx['admission_complete'] = True
    ctx['seed'] = Path(raw['path'])
    ctx['data_records'] = disk_and_artifacts
    admitted.update(receipt=receipt, receipt_sha256=ctx['receipt_sha256'], receipt_bytes=ctx['receipt_bytes'],
        verification_bytes=verification_bytes, manifest_bytes=manifest_bytes,
        consumer_seed=ctx['seed'], verification_sha256=VERIFICATION_SHA, manifest_sha256=MANIFEST_SHA)
    return admitted


def before_launch(ctx, consumer_disk):
    require(ctx['admission_complete'], 'Continuation admission incomplete')
    validate_sources(ctx['contract'])
    require(protected_unchanged(ctx), 'Protected admission inputs changed before launch')
    consumer = file_record(consumer_disk, HDD_SHA, HDD_BYTES)
    require(consumer['mode'] & 0o200, 'Consumer copy must be writable private file')
    independent(ctx['data_records'] + [consumer])
    require_space(ctx, Path(consumer_disk).parent, consumer_copy_needed=False)
    require(not os.path.lexists(PRODUCER / 'consumer-attempt.json'), 'Consumer attempt already reserved')
    ctx['protected_records'].append(consumer)
    return dict(kind='RF_NEW_L3S1_SAVED_CONSUMER_ATTEMPT_V2',
        batch=ctx['contract']['batch']['id'], out=ctx['consumer_out'],
        source_commit=ctx['contract']['consumer_source_commit'], producer_source_commit=PRODUCER_COMMIT,
        producer_sha256=HDD_SHA, consumer_build_sha256=ctx['build_sha256'],
        producer_xbe_sha256=XBE_SHA, consumer_xbe_sha256=ctx['consumer_xbe_sha256'],
        raw_xbe_comparison_sha256=ctx['xbe_comparison_sha256'], structural_policy_sha256=STRUCTURAL_POLICY_SHA,
        actual09_reuse_policy_sha256=ACTUAL09_REUSE_POLICY_SHA,
        artifact_mode=ctx['artifact_mode'], artifact_origin=ctx['artifact_origin'], xbe_admission_branch=ctx['xbe_branch'],
        continuation_contract_sha256=ctx['contract_sha256'], backup_receipt_sha256=ctx['receipt_sha256'],
        seed=dict(path=str(ctx['seed']), sha256=HDD_SHA, bytes=HDD_BYTES))


def report_identity(ctx):
    require(ctx['admission_complete'], 'Continuation admission incomplete')
    return dict(kind='RF_NEW_L3S1_SAVED_CONSUMER_CONTINUATION_V3',
        batch=ctx['contract']['batch']['id'], producer_source_commit=PRODUCER_COMMIT,
        consumer_source_commit=ctx['contract']['consumer_source_commit'],
        consumer_source_tree=ctx['contract']['consumer_source_tree'],
        consumer_build_sha256=ctx['build_sha256'], compatibility_contract_sha256=ctx['contract_sha256'],
        producer_xbe_sha256=XBE_SHA, consumer_xbe_sha256=ctx['consumer_xbe_sha256'],
        raw_xbe_comparison_path=ctx['xbe_comparison_path'], raw_xbe_comparison_sha256=ctx['xbe_comparison_sha256'],
        structural_policy_sha256=STRUCTURAL_POLICY_SHA, runtime_equivalence_claimed=False,
        actual09_reuse_policy_sha256=ACTUAL09_REUSE_POLICY_SHA,
        artifact_mode=ctx['artifact_mode'], artifact_origin=ctx['artifact_origin'], xbe_admission_branch=ctx['xbe_branch'],
        archive_approval_sha256=ctx['approval_sha256'], durable_backup_receipt_sha256=ctx['receipt_sha256'],
        consumer_seed_path=str(ctx['seed']), consumer_seed_sha256=HDD_SHA,
        producer_repeated=False, historical_regression_recovered=False)


def main():
    """Metadata/source-only runner preflight; runtime admission remains separate."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preflight', action='store_true', required=True)
    parser.add_argument('--contract', type=Path, required=True)
    parser.add_argument('--contract-sha256', required=True)
    parser.add_argument('--approval', type=Path, required=True)
    parser.add_argument('--approval-sha256', required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--receipt-sha256', required=True)
    parser.add_argument('--batch', required=True)
    parser.add_argument('--source-pin', required=True)
    parser.add_argument('--source-tree', required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--build-manifest', required=True)
    parser.add_argument('--build-start', required=True)
    parser.add_argument('--build-log', required=True)
    parser.add_argument('--consumer-out', required=True)
    parser.add_argument('--parent-prefix', required=True)
    parser.add_argument('--environment-sha256', required=True)
    args = parser.parse_args()
    ctx = load_context(ROOT, args.contract, args.contract_sha256, args.approval,
                       args.approval_sha256, args.receipt, args.receipt_sha256)
    contract = ctx['contract']
    require(contract['external_source_files'][ENVIRONMENT_SOURCE] == args.environment_sha256,
            'Runner environment pin differs from contract')
    require(args.batch == contract['batch']['id'] and args.source_pin == contract['consumer_source_commit'] and
            args.source_tree == contract['consumer_source_tree'] and args.target == contract['batch']['not_before_utc'],
            'Runner source/hourly identity differs from contract')
    for key, value in (('build_manifest', args.build_manifest), ('build_start', args.build_start), ('build_log', args.build_log),
                       ('consumer_out', args.consumer_out), ('parent_prefix', args.parent_prefix)):
        require(contract['paths'][key] == value, 'Runner target differs from contract: ' + key)
    require(datetime.datetime.now(datetime.timezone.utc) >=
            datetime.datetime.fromisoformat(contract['batch']['not_before_utc']), 'Before approved hourly slot')
    for name in ('build_manifest', 'build_start', 'build_log', 'consumer_out'):
        target = path_checked(contract['paths'][name], exists=False)
        require(not target.exists(), 'Continuation output already exists: ' + name)
    require(not os.path.lexists(PRODUCER / 'consumer-attempt.json'), 'Consumer attempt already reserved')
    terminal_path = Path('/workspace/shared/rf-20261011-0800-parent.exit.json')
    member = ctx['receipt']['proof_member_inventory'].get('parent/' + terminal_path.name)
    keys(member, {'bytes', 'sha256'}, 'Original terminal proof member')
    terminal, unused = read_json(terminal_path, member['sha256'])
    require(terminal_path.stat().st_size == member['bytes'] and terminal.get('terminal') is True and
            type(terminal.get('exit_code')) is int and terminal['exit_code'] == 1 and
            terminal.get('source_commit') == PRODUCER_COMMIT, 'Original terminal08 outcome changed')
    print('PASS_CONTINUATION_SOURCE_AND_RECEIPT_METADATA_PREFLIGHT', flush=True)


if __name__ == '__main__':
    main()
