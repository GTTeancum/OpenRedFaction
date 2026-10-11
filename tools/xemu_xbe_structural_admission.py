"""Source-qualified raw XBE comparison and one normal parent-owned build.

No normalization, rewrite, forced rewrap, build retry or runtime-equivalence claim.
Imported declarations are inert. The CLI is a future explicitly approved runner
entry point; this external candidate has not been imported or executed.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import time
import zipfile

ROOT = Path('/workspace/shared/OpenRedFaction-attack')
SDK = Path('/workspace/shared/recovery/toolchain/nxdk')
SDK_COMMIT = '14d5ee97e73347c973f1f57b68b79ec08c9e77f2'
POLICY_SHA = '5ed12e4c0ab2c276a6675b458f9fe2be605b9501409859ba2cc2f9ed493722e0'
REUSE_POLICY_SHA = '54faa9fe6e37e64d005d963f2c7e72b0ab6a7f1b6b8b3461e117c9769655b1b0'
ORIGIN_COMMIT = '3ed8c4a07a6563707803d6b49531947d2a525b4f'
ORIGIN_TREE = 'fc367092b139d19131338015350a809133901c04'
ORIGIN_XBE_SHA = '32fcd13eb5765831d9b407deffbe86bc6044aeb41d1cf34000e72dc168c18bc0'
ORIGIN_PROOF_PATH = Path('/workspace/shared/OpenRedFaction-hourly-proof-20261011-0900.zip')
ORIGIN_PROOF_SHA = '9e0995abafac3e96c03dfc0152a6c1a3584d0eeeebb0dff9627bbe38911519d8'
ORIGIN_PROOF_BYTES = 2213652
ORIGIN_MEMBERS = {
    'proof-identity.json': (3449, 'c4ad86e83ce4fd5c64a4670ac331ff27adcb6ab9784c3b101fc2e0aa06e7d5b4'),
    'parent/rf-20261011-0900-parent.queued': (81, 'b19284d819f7ca1d53d529090a136ef7a298b0439a3f0573c98f76050cb21814'),
    'parent/rf-20261011-0900-parent.started': (81, '8f2b6afa0e3d675c04984733c15ec075032f079c8028f39525d0eebe583a7591'),
    'parent/rf-20261011-0900-parent.log': (21, 'f777309dd4674338d8372dd492134e0b887f449b03532c1adba55cc7b40a1135'),
    'parent/rf-20261011-0900-parent.exit.json': (467, '9978addc9ffe52e0870bb6c1879c2a0b12931e13a9240fd9f5656beeba85c519'),
    'parent/run-rf-20261011-0900.sh': (1831, '3cb3ba785730e71784038db162963af8d6c8d61515a144e8369a6946c71afc60'),
    'parent/launch-rf-20261011-0900-parent.sh': (1751, '80e080e9724bbb16d9163a14baa03fbae9ab1fe17cd647639105be93653ebb3c'),
    'review/REVIEW.md': (4612, 'b67f4a8a8e48d0404c99e910d6b20b2faddfe6982005d49f44a94c906b9e027e'),
    'review/REVIEW.json': (2649, '3bc7e679216f4bfe5564b7555a8eae5d9bb63598fda5af6ea8cd3b7ba094833f'),
    'orchestration/preserve-rf-20261011-0900-proof.py': (7243, 'f1883468407c0623df0af6e085416cb83663ad99e753c867a031cd0edbfcfb05'),
    'build/20261011-0900-build.log': (120, 'e1c592f86ec52985e86dc6b1cbc1ddb7d877cd082398508df5e05c160e278c1b'),
    'build/rf-20261011-0900-build.json': (342, '21d1f121b788053bd4e91d332eefcacae0cc5ffc5e46211260a981ccde3b76e9'),
    'build/default.xbe': (3948544, ORIGIN_XBE_SHA),
    'build/main.map': (1082108, '14dc720e8959b0a041ab27a41f633a69fce0298ed5599bc350367b10a4d84ef7'),
}
REFERENCE = Path('/workspace/shared/rf-natural-save-producer-20261011-0800')
XBE_SHA = '809b98a9cba6e21b3e8a00826f156c0e87fbc788914fbf0bfbfc47c2d9cdb4fd'
MAP_SHA = '14dc720e8959b0a041ab27a41f633a69fce0298ed5599bc350367b10a4d84ef7'
XBE_BYTES = 3948544
MAP_BYTES = 1082108
EXTERNAL_PINS = {
    '/workspace/shared/recovery/toolchain/environment.sh': '40cec51ed082f6173c7a5a16faf803bb05b778faee483c4798436644575b8037',
    str(SDK / 'tools/cxbe/cxbe'): '5d202eee83659c4bf1b22a66c3474e8f83871f445248dc59a25183d0f21d2cda',
    str(SDK / 'tools/cxbe/Xbe.cpp'): '48a88e6455c4fe263b6040f0328a866a3c7be10d6434cf83e0081d2e1728dd9c',
    str(SDK / 'tools/cxbe/Xbe.h'): '9f4ae3e504511faff572db29487b2d4107350566eb76cc25fdde7a5cf6d42668',
    str(SDK / 'tools/cxbe/Makefile'): 'e6ce8038043a052379f5758a02f3b5738d7e9a7552e1f5eb53510e6d389e4e43',
    str(SDK / 'Makefile'): '3d1db29fe9f34b1cdc668400f03c8aa3a55e87970b8e6a330b1c9f303a2e9a2b',
    '/usr/bin/make': '8a229baf64671988cfb5af43ba3070c6ce7214cf32bcf96155115559d1466fa7',
    '/usr/bin/bash': '223fe8564b60636bc738b6178d3ba9a50ef7d791266b0efae6363bb716e4c47f',
    str(SDK / 'lib/Makefile'): 'e368ad705f2d830d94adfb12c3271ce99f6591695b8ce69f087adb1082e268c0',
    str(SDK / 'lib/Makefile-pdclib'): '5880cbe53633abaf820b4122199b8bfa56ab5f99347284ce90a47ac61531aa9f',
    str(SDK / 'lib/winapi/Makefile'): '4ea9691d1e9e28d17894d4b13f90221fab4522217259621266f46d1f9274bf43',
    str(SDK / 'lib/hal/Makefile'): 'bd9c4b09ec5cf9dc9614b8db2ad675cf99cd7a41b8c68223f64ab22262963e0c',
    str(SDK / 'lib/nxdk/Makefile'): '82f34b786e6cfd9349f40bb2f5ed0ce81e80fea2fc5c626eda8858ec123010a7',
    str(SDK / 'lib/pbkit/Makefile'): '96700219916f98897c7c1037efdbb7615c32d1f08609e1c6a0e9a66c1bb5281f',
    str(SDK / 'lib/usb/Makefile'): 'dd97371b0ad193cac9bf501d5052e2aa29028ae0aea0d0f8bba55608d430e1d0',
    str(SDK / 'lib/xboxrt/Makefile'): '06096ac1de67211c3b2de7f2f5bbeb62b8867f8ea9a62405d9364832049f65a9',
    str(SDK / 'lib/zlib/Makefile'): '6ef6f385855fe014e2cc82cee9e7f8c251253d57f734406add65e021295de299',
    str(SDK / 'lib/libpng/Makefile'): '21cec9061f0ceb25f8318b30e06c3054fc3bf99cfbf49d2408a09acf493c1c56',
    str(SDK / 'lib/libjpeg/Makefile'): '7f03bbe1836dd41a8120bec38e450ab4b70abc1abea4037952f996b232b1363e',
    str(SDK / 'lib/xboxkrnl/Makefile'): 'd8fd45c98fce516e1fe037ab5ac7805cbed08ce22526df99cce49e780845dae0',
    str(SDK / 'lib/net/Makefile'): 'fd5ac6565070aecda06c2266ce02cdcd57f21945d2c574b7a8ff4c70bf3a546e',
    str(SDK / 'lib/sdl/Makefile'): '5ef61ad563e18d7c88f986b3fa414ff50d14bd626e8caa282884a714b742c547',
    str(SDK / 'lib/winapi/winmm/Makefile'): '2807e79c403ba1cdc22d82d1d30adbe1dccb9bcfba0c2d371b777233deac8032',
    str(SDK / 'lib/sdl/SDL2/Makefile.xbox'): '7bbe53e6c29f28fcb2e166ed7817a047c9ab9687b5375dd51b69607f0bb3882e',
    str(SDK / 'lib/net/lwip/src/Filelists.mk'): 'a750091212a64e856461256dc27fd749f7851780bf70ca3b67751afe36f2699e',
}
SDK_SOURCES = ('Makefile', 'tools/cxbe/Xbe.cpp', 'tools/cxbe/Xbe.h', 'tools/cxbe/Makefile')
FORBIDDEN_ENV = ('MAKEFLAGS', 'MFLAGS', 'GNUMAKEFLAGS', 'MAKEOVERRIDES', 'MAKEFILES',
    'MAKELEVEL', 'CXBE', 'BASH_ENV', 'ENV', 'SHELLOPTS', 'BASHOPTS', 'LD_PRELOAD',
    'LD_AUDIT', 'CDPATH', 'NXDK_ONLY', 'DEBUG', 'LTO', 'V')
BUILD_KEYS = {'batch', 'source_tree', 'parent_runner_pid', 'parent_runner_start_ticks',
    'stock_profile', 'status', 'source_commit', 'xbe', 'map', 'runtime', 'build_start_sha256',
    'producing_build', 'xbe_record', 'map_record', 'build_log_record', 'artifact_mode',
    'artifact_origin', 'xbe_observation', 'map_observation'}
INTERVAL_KEYS = {'wall_start_ns', 'wall_end_ns', 'monotonic_start_ns', 'monotonic_end_ns',
    'builder_path', 'builder_sha256', 'policy_sha256', 'tool_records', 'cxbe_steps',
    'build_driver_pid', 'build_driver_start_ticks', 'dependencies', 'actual09_reuse_policy_sha256'}
START_KEYS = {'kind', 'batch', 'source_commit', 'source_tree', 'parent_runner_pid',
    'parent_runner_start_ticks', 'build_driver_pid', 'build_driver_start_ticks',
    'continuation_contract_sha256', 'policy_sha256', 'wall_start_ns', 'monotonic_start_ns',
    'tool_records', 'before_xbe', 'before_map', 'before_dependencies', 'command', 'build_environment',
    'before_xbe_observation', 'before_map_observation', 'actual09_reuse_policy_sha256', 'actual09_origin'}
SECTIONS = (
    (b'.text', 6, 0x11000, 0x399000, 0x1000, 0x398e00, '976656b808e7809129caa303264cd1fa4100b1f108950d868c67327a876ac86b'),
    (b'.rdata', 2, 0x3aa000, 0x26000, 0x39a000, 0x25b34, '5338f49945dd42bf37e44ecec5ac0d20eed8d4389a33dcb507c9024669c0af8a'),
    (b'.data', 3, 0x3d0000, 0x182000, 0x3c0000, 0x21fc, '1d0a6991b105ebc394e65fb5e4fcaadade618eb95aac9b1706ed1a1f9245be71'),
    (b'.tls', 3, 0x552000, 0x118, 0x3c3000, 4, 'df3f619804a92fdb4057192dc43dd748ea778adc52bc498ce80524c014b81119'),
)


def require(ok, why):
    if not ok:
        raise RuntimeError('XBE structural admission: ' + why)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(c, path, limit, expected=None, with_observation=False):
    """All comparisons, hashes and decoding consume this one stable full buffer."""
    path = c.path_checked(path)
    before = c.regular(path)
    identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mode, s.st_nlink, s.st_mtime_ns, s.st_ctime_ns)
    require(before.st_size <= limit, 'Input exceeds exact bound: ' + str(path))
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW), 'rb') as stream:
        opened = os.fstat(stream.fileno())
        require(identity(opened) == identity(before), 'Input replaced before open')
        data = stream.read(limit + 1)
        require(len(data) == opened.st_size and len(data) <= limit, 'Short/oversized input')
        after_fd = os.fstat(stream.fileno())
    require(identity(before) == identity(after_fd) == identity(c.regular(path)), 'Unstable full-file snapshot')
    digest = sha(data)
    require(expected is None or digest == expected, 'Unexpected raw bytes: ' + str(path))
    record = dict(path=str(path), sha256=digest, bytes=len(data), device=before.st_dev,
                  inode=before.st_ino, mode=stat.S_IMODE(before.st_mode))
    if with_observation:
        return data, record, dict(record, mtime_ns=before.st_mtime_ns, ctime_ns=before.st_ctime_ns,
                                 links=before.st_nlink)
    return data, record


def tool_records(c, contract):
    c.keys(contract['external_source_files'], set(EXTERNAL_PINS), 'Structural build-source pins')
    require(c.same_json(contract['external_source_files'], EXTERNAL_PINS), 'Reviewed builder/source pins differ')
    require(c.git(SDK, 'rev-parse', 'HEAD').decode().strip() == SDK_COMMIT, 'NXDK commit differs')
    records = [c.file_record(path, expected) for path, expected in EXTERNAL_PINS.items()]
    for name in SDK_SOURCES:
        require(sha(c.git(SDK, 'show', SDK_COMMIT + ':' + name)) == EXTERNAL_PINS[str(SDK / name)],
                'Selected SDK source is not the reviewed commit blob')
    # The whole SDK has generated files/submodule status; no cleanliness claim.
    return records


def parent_identity(pid):
    require(type(pid) is int and pid > 0, 'Invalid live parent PID')
    return int(Path('/proc/' + str(pid) + '/stat').read_text().rsplit(')', 1)[1].split()[19])


def declared_file_record(c, value, path, limit):
    c.keys(value, {'path', 'sha256', 'bytes', 'device', 'inode', 'mode'}, 'Build file record')
    require(value['path'] == str(path), 'Wrong build artifact path')
    c.hash_value(value['sha256'], 'Build artifact raw hash')
    for key in ('bytes', 'device', 'inode', 'mode'):
        c.number(value[key], 'Build artifact ' + key)
    require(value['bytes'] <= limit and value['inode'] > 0 and value['mode'] <= 0o7777,
            'Invalid declared artifact bounds/identity')


def declared_observation(c, value, record):
    c.keys(value, set(record) | {'mtime_ns', 'ctime_ns', 'links'}, 'Stable artifact observation')
    require(c.same_json({key: value[key] for key in record}, record), 'Observation/file record differs')
    for key in ('mtime_ns', 'ctime_ns', 'links'):
        c.number(value[key], 'Artifact ' + key, positive=True)
    require(value['links'] == 1, 'Active XBE/map must not share a hardlink')


def actual09_origin(c, contract):
    """Authenticate fixed local origin evidence, never a generic prior build."""
    require(contract['actual09_reuse_policy_sha256'] == REUSE_POLICY_SHA, 'Wrong exact09 reuse design')
    require(c.git(ROOT, 'rev-parse', ORIGIN_COMMIT + '^{tree}').decode().strip() == ORIGIN_TREE,
            'Actual09 producing source tree differs')
    producer_tree, origin_tree = c.tree(ROOT, c.PRODUCER_COMMIT), c.tree(ROOT, ORIGIN_COMMIT)
    changed = {name for name in set(producer_tree) | set(origin_tree)
               if producer_tree.get(name) != origin_tree.get(name)}
    require(changed == {'TO-DO.MD', 'docs/HOURLY-20261011-0800.md'}, 'Actual08-to09 is not the fixed docs-only relation')
    for name in changed:
        require(all(entry is None or entry['mode'] == '100644'
                    for entry in (producer_tree.get(name), origin_tree.get(name))), 'Historical source mode differs')
    # Current source is independently whole-tree checked by validate_sources;
    # fixed08->09 plus reviewed08->current establishes the full transitive join.
    proof_bytes, proof_record = snapshot(c, ORIGIN_PROOF_PATH, ORIGIN_PROOF_BYTES, ORIGIN_PROOF_SHA)
    require(len(proof_bytes) == ORIGIN_PROOF_BYTES, 'Wrong origin proof length')
    members = {}
    with zipfile.ZipFile(io.BytesIO(proof_bytes)) as archive:
        infos = archive.infolist()
        require(len(infos) == 14 and len({item.filename for item in infos}) == 14 and
                {item.filename for item in infos} == set(ORIGIN_MEMBERS), 'Wrong complete14-member origin proof')
        for item in infos:
            size, expected = ORIGIN_MEMBERS[item.filename]
            require(not item.is_dir() and not item.flag_bits & 1 and item.file_size == size,
                    'Invalid origin proof member')
            with archive.open(item) as stream:
                data = stream.read(size + 1)
            require(len(data) == size and sha(data) == expected, 'Origin member differs: ' + item.filename)
            members[item.filename] = data
    identity = c.strict_json(members['proof-identity.json'])
    c.keys(identity, {'status', 'source_commit', 'parent_terminal', 'runtime', 'build', 'artifacts',
                     'build_log_present', 'build_manifest', 'coverage_note', 'members'}, 'Actual09 proof identity')
    expected_inventory = {name: dict(bytes=size, sha256=digest) for name, (size, digest) in ORIGIN_MEMBERS.items()
                          if name != 'proof-identity.json'}
    require(c.same_json(identity['members'], expected_inventory) and
            identity['status'] == 'PASS_XBOX_BUILD_ONLY' and identity['source_commit'] == ORIGIN_COMMIT and
            identity['runtime'] == 'NOT_RUN' and identity['build'] == 'PASS_XBOX_BUILD' and
            identity['artifacts'] == 'EXACT_MANIFEST_HASHES_VERIFIED' and identity['build_log_present'] is True and
            type(identity['coverage_note']) is str and identity['coverage_note'], 'Wrong actual09 proof meaning')
    build = c.strict_json(members['build/rf-20261011-0900-build.json'])
    expected_build = dict(status='PASS_XBOX_BUILD', source_commit=ORIGIN_COMMIT, xbe=ORIGIN_XBE_SHA,
        map=MAP_SHA, runtime='NOT_RUN_BUILD_ONLY_FALLBACK_BACKUP_PERMISSION_OR_CONSUMER_ADMISSION_PENDING')
    require(c.same_json(build, expected_build) and c.same_json(identity['build_manifest'], build),
            'Original09 build manifest was relabeled')
    terminal = c.strict_json(members['parent/rf-20261011-0900-parent.exit.json'])
    expected_terminal = dict(terminal=True, source_commit=ORIGIN_COMMIT, exit_code=0,
        launcher_path='/workspace/shared/launch-rf-20261011-0900-parent.sh',
        launcher_sha256=ORIGIN_MEMBERS['parent/launch-rf-20261011-0900-parent.sh'][1],
        runner_path='/workspace/shared/run-rf-20261011-0900.sh',
        runner_sha256=ORIGIN_MEMBERS['parent/run-rf-20261011-0900.sh'][1],
        ended_utc='2026-10-11T09:00:02.473116+00:00')
    require(c.same_json(terminal, expected_terminal) and c.same_json(identity['parent_terminal'], terminal),
            'Original09 parent outcome differs')
    started = b'2026-10-11T09:00:00,718232915+00:00 pin=' + ORIGIN_COMMIT.encode('ascii') + b'\n'
    require(members['parent/rf-20261011-0900-parent.started'] == started, 'Wrong original producing interval')
    expected_log = ('[ CXBE     ] ' + str(ROOT / 'build/xbox/disc/default.xbe') +
                    '\n[ XISO     ] redfaction-diagnostic.iso\n').encode('ascii')
    require(members['build/20261011-0900-build.log'] == expected_log, 'Wrong preserved actual09 production log')
    records = [proof_record]
    for name, local in (
            ('build/rf-20261011-0900-build.json', c.WORKSPACE_BUILD_PATH),
            ('build/20261011-0900-build.log', ROOT / 'artifacts/hourly/20261011-0900-build.log'),
            ('parent/rf-20261011-0900-parent.started', Path('/workspace/shared/rf-20261011-0900-parent.started')),
            ('parent/rf-20261011-0900-parent.exit.json', Path('/workspace/shared/rf-20261011-0900-parent.exit.json')),
            ('parent/run-rf-20261011-0900.sh', Path(expected_terminal['runner_path'])),
            ('parent/launch-rf-20261011-0900-parent.sh', Path(expected_terminal['launcher_path']))):
        data, record = snapshot(c, local, ORIGIN_MEMBERS[name][0], ORIGIN_MEMBERS[name][1])
        require(data == members[name], 'Preserved local origin record differs from exact proof')
        records.append(record)
    qualification_paths = {
        '/workspace/shared/actual08-09-xbe-qualification-20261011.json': '5058a8d1be939a6f9eb95392d75bf80ffa48ed42c2eb670453108d243365569a',
        '/workspace/shared/actual08-09-xbe-qualification-20261011.md': '0c2e5d52aff3a7e950e3f211f39dfcb987c21ef90a7df2a64c0d5a8467f1dcfb',
        '/workspace/shared/xbe-build-difference-independent-review-20261011-0904.md': 'f793cd2c65a0f989f046c989da3b9cab079f351170b8283ae8404e43b53878dc',
    }
    records.extend(c.file_record(path, expected) for path, expected in qualification_paths.items())
    origin = dict(kind='RF_EXACT_ACTUAL09_ARTIFACT_ORIGIN_V1', source_commit=ORIGIN_COMMIT,
        source_tree=ORIGIN_TREE, xbe_sha256=ORIGIN_XBE_SHA, xbe_bytes=XBE_BYTES, map_sha256=MAP_SHA,
        map_bytes=MAP_BYTES, timestamp=0x6acb5011,
        parent_interval=dict(start_ns=1791709200718232915, end_ns=1791709202473116000,
            scope='ORIGINAL09_PARENT_INTERVAL_NOT_A_NEW_CXBE_MEASUREMENT'),
        build_manifest_sha256=ORIGIN_MEMBERS['build/rf-20261011-0900-build.json'][1],
        parent_started_sha256=ORIGIN_MEMBERS['parent/rf-20261011-0900-parent.started'][1],
        parent_terminal_sha256=ORIGIN_MEMBERS['parent/rf-20261011-0900-parent.exit.json'][1],
        proof=dict(path=str(ORIGIN_PROOF_PATH), sha256=ORIGIN_PROOF_SHA, bytes=ORIGIN_PROOF_BYTES),
        qualification_records=qualification_paths, original_runtime='NOT_RUN',
        historical_cxbe_invocation_hash_attested=False, actual09_reuse_policy_sha256=REUSE_POLICY_SHA)
    return origin, records, proof_bytes


def timed_artifacts_unchanged(c, observations):
    for prior in observations:
        unused, record, current = snapshot(c, prior['path'], prior['bytes'], prior['sha256'], with_observation=True)
        if not c.same_json(prior, current):
            return False
    return True


def build_environment():
    # Reject inherited make/shell control, then use a closed build environment.
    for name in FORBIDDEN_ENV:
        require(name not in os.environ, 'Inherited build override: ' + name)
    require(not any(name.startswith('BASH_FUNC_') for name in os.environ), 'Exported shell function')
    recovery = '/workspace/shared/recovery/toolchain'
    debian = recovery + '/debian/root'
    return dict(PATH=str(SDK / 'bin') + ':' + debian + '/usr/lib/llvm-19/bin:' + debian + '/usr/bin:/usr/bin:/bin',
        NXDK_DIR=str(SDK), RF_RECOVERY_TOOLCHAIN=recovery, RF_DEBIAN_PREFIX=debian,
        LD_LIBRARY_PATH=debian + '/usr/lib/x86_64-linux-gnu',
        LIBRARY_PATH=debian + '/usr/lib/x86_64-linux-gnu', BISON_PKGDATADIR=debian + '/usr/share/bison',
        M4=debian + '/usr/bin/m4', RF_GEOMOD_EXPANDED_PROFILE='0', LANG='C', LC_ALL='C')


def make_resolution():
    # Wrapper prepends these paths. A make earlier than the pinned /usr/bin/make
    # would invalidate the reviewed CXBE assignment/target resolution.
    for directory in (SDK / 'bin', Path('/clang64/bin'), Path('/mingw64/bin')):
        require(not os.path.lexists(directory / 'make'), 'Unreviewed make shadows /usr/bin/make')


def dependency_records(c):
    """Accept data-only compiler .d syntax; never evaluate it as make code.

    These current generated files are not historical build lineage. The static
    reviewed include closure determines SRCS beneath these three source roots.
    Both pre/post inventories are measured; ordinary compiler changes are allowed.
    """
    roots = (ROOT / 'src', ROOT / 'build/nxaudio', SDK / 'lib')
    found = sorted(path for root in roots for path in root.rglob('*')
                   if path.name.endswith(('.c.d', '.cpp.d')))
    require(len(found) <= 4096, 'Generated dependency count exceeds bound')
    records, total = [], 0
    for path in found:
        data, record = snapshot(c, path, 1024**2)
        total += len(data)
        require(total <= 64 * 1024**2, 'Generated dependency bytes exceed bound')
        text = data.decode('ascii').replace('\\\n', ' ')
        require(not any(character in text for character in '$#;=|%`\\\t\r\x00'),
                'Executable/unsupported make syntax in dependency file: ' + str(path))
        lines = [line.strip() for line in text.split('\n') if line.strip()]
        require(lines, 'Empty dependency file')
        suffix = '.cpp.d' if path.name.endswith('.cpp.d') else '.c.d'
        expected_target = str(path)[:-len(suffix)] + '.obj'
        dependencies, seen = set(), set()
        for index, line in enumerate(lines):
            require(line.count(':') == 1, 'Unsupported dependency target syntax')
            target, prerequisites = line.split(':')
            target = target.strip()
            tokens = prerequisites.split()
            require(re.fullmatch(r'/[A-Za-z0-9_./+\-]+', target) is not None and
                    all(re.fullmatch(r'/[A-Za-z0-9_./+\-]+', token) is not None for token in tokens),
                    'Nonliteral dependency path')
            require(target not in seen, 'Duplicate dependency target')
            seen.add(target)
            if index == 0:
                require(target == expected_target and tokens, 'Dependency file targets another build rule')
                dependencies = set(tokens)
                require(str(path)[:-2] in dependencies, 'Dependency declaration omits its owning source')
            else:
                require(target in dependencies and not tokens, 'Extra target/recipe in generated dependencies')
        records.append(record)
    return records


def dependency_identity(c, records):
    return dict(count=len(records), inventory_sha256=sha(c.canonical(records)))


def declared_dependencies(c, value):
    c.keys(value, {'count', 'inventory_sha256'}, 'Generated dependency inventory')
    c.number(value['count'], 'Generated dependency count')
    require(value['count'] <= 4096, 'Declared dependency count exceeds bound')
    c.hash_value(value['inventory_sha256'], 'Dependency inventory hash')


def exclusive_bytes(c, path, data):
    path = c.path_checked(path, exists=False)
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600), 'wb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)
    return c.file_record(path, sha(data))


def exclusive_json(c, path, value):
    return exclusive_bytes(c, path, c.canonical(value) + b'\n')


def inspect_layout(data):
    require(len(data) == XBE_BYTES and data[:4] == b'XBEH', 'Wrong complete XBE length/magic')
    word = lambda offset: struct.unpack_from('<I', data, offset)[0]
    for offset, expected in ((0x104, 0x10000), (0x108, 0x5b9), (0x110, 0x178),
            (0x118, 0x10178), (0x11c, 4), (0x120, 0x10348), (0x178, 0x1d0),
            (0x160, 1), (0x164, 0x1044a)):
        require(word(offset) == expected, 'Changed fixed layout at ' + hex(offset))
    require(data[0x44a:0x450] == b'CXBE0\0' and data[0x452:0x45a] == bytes(8),
            'Changed sole library prefix/terminator/version/flags')
    require(word(0x114) == word(0x17c), 'Header/certificate times differ')
    section_records = []
    end = 0x1000
    for index, (name, flags, address, virtual_size, raw, size, digest) in enumerate(SECTIONS):
        offset = 0x348 + index * 56
        fields = struct.unpack_from('<9I20s', data, offset)
        require(fields[:5] == (flags, address, virtual_size, raw, size), 'Changed section layout')
        name_offset = fields[5] - 0x10000
        require(0 <= name_offset < 0x5b9 and name_offset + len(name) + 1 <= 0x5b9,
                'Invalid section-name pointer')
        require(data[name_offset:name_offset + len(name) + 1] == name + b'\0', 'Changed section name')
        require(raw >= end and raw + size <= len(data) and size <= virtual_size,
                'Overlapping/out-of-bounds section')
        end = raw + size
        require(sha(data[raw:end]) == digest, 'Changed complete section bytes')
        section_records.append(dict(name=name.decode('ascii'), raw_offset=raw, raw_bytes=size,
                                    sha256=digest, descriptor_sha256=sha(data[offset:offset + 56])))
    require(sha(data[0x1000:]) == '5a57961b5157a0471ad65162fe15498c134a43d3c3d443bd04ad74f93fe96b69',
            'Changed full payload/padding/tail')
    return word(0x114), section_records


def validate_build(c, ctx, build, build_bytes):
    contract = ctx['contract']
    c.keys(build, BUILD_KEYS, 'Actual structural consumer build')
    interval = c.keys(build['producing_build'], INTERVAL_KEYS, 'Measured producing-build interval')
    start, start_bytes = c.read_json(contract['paths']['build_start'], build['build_start_sha256'])
    c.keys(start, START_KEYS, 'Owned build-start record')
    require(start['kind'] == 'RF_SAVED_CONSUMER_BUILD_START_V1', 'Wrong start kind')
    declared_file_record(c, start['before_xbe'], ROOT / 'build/xbox/disc/default.xbe', XBE_BYTES)
    declared_file_record(c, start['before_map'], ROOT / 'build/xbox/main.map', MAP_BYTES)
    declared_file_record(c, build['xbe_record'], ROOT / 'build/xbox/disc/default.xbe', XBE_BYTES)
    declared_file_record(c, build['map_record'], ROOT / 'build/xbox/main.map', MAP_BYTES)
    declared_file_record(c, build['build_log_record'], contract['paths']['build_log'], 64 * 1024**2)
    declared_observation(c, start['before_xbe_observation'], start['before_xbe'])
    declared_observation(c, start['before_map_observation'], start['before_map'])
    declared_observation(c, build['xbe_observation'], build['xbe_record'])
    declared_observation(c, build['map_observation'], build['map_record'])
    declared_dependencies(c, start['before_dependencies'])
    declared_dependencies(c, interval['dependencies'])
    for key in ('batch', 'source_commit', 'source_tree', 'parent_runner_pid', 'parent_runner_start_ticks'):
        require(c.same_json(start[key], build[key]), 'Start/finish owner mismatch: ' + key)
    require(start['continuation_contract_sha256'] == ctx['contract_sha256'] and
            start['policy_sha256'] == interval['policy_sha256'] == POLICY_SHA, 'Wrong build policy/contract')
    require(start['actual09_reuse_policy_sha256'] == interval['actual09_reuse_policy_sha256'] ==
            contract['actual09_reuse_policy_sha256'] == REUSE_POLICY_SHA, 'Wrong exact09 reuse policy')
    origin, origin_records, origin_proof = actual09_origin(c, contract)
    require(c.same_json(start['actual09_origin'], origin), 'Build-start origin evidence differs')
    ctx['actual09_origin'] = origin
    ctx['actual09_origin_proof_bytes'] = origin_proof
    require(start['command'] == ['/usr/bin/bash', '--noprofile', '--norc', 'tools/build-xbox.sh'] and
            c.same_json(start['build_environment'], build_environment()), 'Different actual normal build command/environment')
    for name in ('wall_start_ns', 'wall_end_ns', 'monotonic_start_ns', 'monotonic_end_ns',
                 'build_driver_pid', 'build_driver_start_ticks', 'cxbe_steps'):
        c.number(interval[name], name, positive=name != 'cxbe_steps')
    for name in ('wall_start_ns', 'monotonic_start_ns', 'build_driver_pid', 'build_driver_start_ticks'):
        require(type(start[name]) is int and start[name] == interval[name], 'Start/finish interval mismatch')
    elapsed = interval['wall_end_ns'] - interval['wall_start_ns']
    monotonic = interval['monotonic_end_ns'] - interval['monotonic_start_ns']
    require(elapsed >= 0 and monotonic > 0 and abs(elapsed - monotonic) <= 100000000,
            'Invalid/reversed/clock-adjusted producing interval')
    require(interval['wall_end_ns'] <= time.time_ns(), 'Future build interval')
    require(interval['builder_path'] == str(SDK / 'tools/cxbe/cxbe') and
            interval['builder_sha256'] == EXTERNAL_PINS[interval['builder_path']], 'Wrong actual builder')
    actual_tools = tool_records(c, contract)
    require(c.same_json(start['tool_records'], interval['tool_records']) and
            c.same_json(interval['tool_records'], actual_tools), 'Build-time tool identities changed')
    dependencies = dependency_records(c)
    require(c.same_json(interval['dependencies'], dependency_identity(c, dependencies)),
            'Generated make dependencies changed since actual build')
    ctx['dependency_records'] = dependencies
    make_resolution()
    log_bytes, log_record = snapshot(c, contract['paths']['build_log'], 64 * 1024**2)
    require(c.same_json(log_record, build['build_log_record']), 'Owned build log changed')
    cxbe_lines = [line for line in log_bytes.splitlines() if b'[ CXBE' in line]
    exact_line = ('[ CXBE     ] ' + str(ROOT / 'build/xbox/disc/default.xbe')).encode('ascii')
    require(cxbe_lines in ([], [exact_line]) and len(cxbe_lines) == interval['cxbe_steps'],
            'Unexpected/multiple CXBE production steps')
    require(build['parent_runner_pid'] == os.getppid() and
            build['parent_runner_start_ticks'] == parent_identity(os.getppid()), 'Wrong live one-shot build parent')
    records = [c.file_record(contract['paths']['build_start'], sha(start_bytes)), log_record] + actual_tools + dependencies + origin_records
    return start, interval, records


def artifact_identity(c, contract, start, xbe_observation, map_observation, cxbe_steps, origin):
    """Honest build-manifest classification; structural qualification is later."""
    raw_hash = xbe_observation['sha256']
    if raw_hash == XBE_SHA:
        return 'EXACT_ORIGINAL_RAW_XBE', dict(kind='IDENTICAL_TO_ORIGINAL08_RAW_IDENTITY',
            reference_source_commit=c.PRODUCER_COMMIT, reference_xbe_sha256=XBE_SHA,
            new_production_claimed=False)
    if raw_hash == ORIGIN_XBE_SHA and map_observation['sha256'] == MAP_SHA and cxbe_steps == 0 and \
            c.same_json(start['before_xbe_observation'], xbe_observation) and \
            c.same_json(start['before_map_observation'], map_observation):
        return 'EXACT_ACTUAL09_UNCHANGED_REUSE', origin
    return 'FRESH_STRUCTURAL_CANDIDATE', dict(kind='CURRENT_BUILD_REQUIRES_FRESH_STRUCTURAL_ADMISSION',
        source_commit=contract['consumer_source_commit'], source_tree=contract['consumer_source_tree'],
        policy_sha256=POLICY_SHA, qualification_pending=True)


def qualify(c, ctx, build, build_bytes):
    start, interval, records = validate_build(c, ctx, build, build_bytes)
    original, reference = snapshot(c, REFERENCE / 'tested-default.xbe', XBE_BYTES, XBE_SHA)
    candidate, current, xbe_observation = snapshot(c, ROOT / 'build/xbox/disc/default.xbe', XBE_BYTES,
                                                  build['xbe'], with_observation=True)
    old_map, reference_map = snapshot(c, REFERENCE / 'tested-main.map', MAP_BYTES, MAP_SHA)
    new_map, current_map, map_observation = snapshot(c, ROOT / 'build/xbox/main.map', MAP_BYTES,
                                                   MAP_SHA, with_observation=True)
    require(old_map == new_map and len(new_map) == MAP_BYTES, 'Maps are not exact full-byte equals')
    require(c.same_json(build['xbe_record'], current) and c.same_json(build['map_record'], current_map),
            'Current raw build artifact identity changed')
    require(c.same_json(build['xbe_observation'], xbe_observation) and
            c.same_json(build['map_observation'], map_observation), 'Post-build artifact times/in-place identity changed')
    mode, artifact_origin = artifact_identity(c, ctx['contract'], start, xbe_observation,
                                            map_observation, interval['cxbe_steps'], ctx['actual09_origin'])
    require(build['artifact_mode'] == mode and c.same_json(build['artifact_origin'], artifact_origin),
            'Actual build classification/origin differs from independently observed artifacts')
    copied_xbe, copied_xbe_record = snapshot(c, Path(ctx['consumer_out']) / 'tested-default.xbe', XBE_BYTES, build['xbe'])
    copied_map, copied_map_record = snapshot(c, Path(ctx['consumer_out']) / 'tested-main.map', MAP_BYTES, MAP_SHA)
    require(copied_xbe == candidate and copied_map == new_map, 'Retained actual tested artifact differs')
    old_time, old_sections = inspect_layout(original)
    new_time, new_sections = inspect_layout(candidate)
    require(old_time == 0x6acb4222 and old_sections == new_sections, 'Original identity/section descriptors differ')
    ranges = ((0, 0x114), (0x118, 0x17c), (0x180, 0x450), (0x452, XBE_BYTES))
    protected = []
    for begin, end in ranges:
        require(original[begin:end] == candidate[begin:end], 'Difference outside exact ten allowed bytes')
        protected.append(dict(begin=begin, end_exclusive=end, sha256=sha(candidate[begin:end])))
    differences = [dict(offset=i, producer_byte=a, consumer_byte=b)
                   for i, (a, b) in enumerate(zip(original, candidate)) if a != b]
    if mode == 'EXACT_ORIGINAL_RAW_XBE':
        require(candidate == original and not differences, 'Exact branch is not exact')
        branch = 'EXACT_ORIGINAL_RAW_XBE'
    elif mode == 'EXACT_ACTUAL09_UNCHANGED_REUSE':
        require(current['sha256'] == ORIGIN_XBE_SHA and new_time == 0x6acb5011 and
                interval['cxbe_steps'] == 0 and c.same_json(start['before_xbe_observation'], xbe_observation) and
                c.same_json(start['before_map_observation'], map_observation), 'Exact09 in-place reuse was not demonstrated')
        original_interval = artifact_origin['parent_interval']
        require(original_interval['start_ns'] // 1000000000 <= new_time <= original_interval['end_ns'] // 1000000000,
                'Actual09 fixed timestamp differs from its own preserved parent interval')
        branch = 'EXACT_ACTUAL09_UNCHANGED_REUSE'
    else:
        require(interval['cxbe_steps'] == 1, 'Changed-header branch lacks an actual CXBE step in this build')
        require(interval['wall_start_ns'] // 1000000000 <= new_time <= interval['wall_end_ns'] // 1000000000,
                'Changed-header timestamp is outside its own producing build interval')
        require(start['before_xbe']['sha256'] != current['sha256'],
                'Changed-header artifact was reused without observable new production')
        branch = 'SOURCE_QUALIFIED_TEN_BYTE_STRUCTURAL_PREDICATE'
    result = dict(kind='RF_SAVED_CONSUMER_RAW_XBE_COMPARISON_V1',
        status='PASS_SOURCE_QUALIFIED_XBE_FOR_FIRST_TEST', branch=branch,
        policy_sha256=POLICY_SHA, continuation_contract_sha256=ctx['contract_sha256'],
        actual09_reuse_policy_sha256=REUSE_POLICY_SHA, artifact_mode=mode, artifact_origin=artifact_origin,
        source_commit=ctx['contract']['consumer_source_commit'], source_tree=ctx['contract']['consumer_source_tree'],
        batch=build['batch'], parent_runner_pid=build['parent_runner_pid'],
        parent_runner_start_ticks=build['parent_runner_start_ticks'], build_manifest_sha256=sha(build_bytes),
        build_start_sha256=build['build_start_sha256'], producing_build=interval,
        producer_xbe=reference, consumer_xbe=current, producer_map=reference_map, consumer_map=current_map,
        before_xbe_observation=start['before_xbe_observation'], before_map_observation=start['before_map_observation'],
        consumer_xbe_observation=xbe_observation, consumer_map_observation=map_observation,
        producer_timestamp=old_time, consumer_timestamp=new_time, differences=differences,
        protected_ranges=protected, sections=new_sections, runtime_equivalence_claimed=False)
    comparison_path = Path(ctx['consumer_out']) / 'raw-xbe-comparison.json'
    receipt = exclusive_json(c, comparison_path, result)
    records.append(exclusive_json(c, Path(ctx['consumer_out']) / 'producing-dependencies.json', ctx['dependency_records']))
    records.append(exclusive_bytes(c, Path(ctx['consumer_out']) / 'actual09-origin-proof.zip', ctx['actual09_origin_proof_bytes']))
    records.append(exclusive_json(c, Path(ctx['consumer_out']) / 'actual09-origin.json', ctx['actual09_origin']))
    for source, expected in ctx['actual09_origin']['qualification_records'].items():
        data, unused = snapshot(c, source, 1024**2, expected)
        records.append(exclusive_bytes(c, Path(ctx['consumer_out']) / Path(source).name, data))
    # Preserve actual owned evidence bytes inside the eventual consumer proof.
    for source, name, expected, limit in (
            (ctx['contract']['paths']['build_start'], 'producing-build-start.json', build['build_start_sha256'], 262144),
            (ctx['contract']['paths']['build_log'], 'producing-build.log', build['build_log_record']['sha256'], 64 * 1024**2)):
        data, unused = snapshot(c, source, limit, expected)
        records.append(exclusive_bytes(c, Path(ctx['consumer_out']) / name, data))
    ctx['protected_records'].extend(records + [reference, current, reference_map, current_map,
                                              copied_xbe_record, copied_map_record, receipt])
    ctx['consumer_xbe_sha256'] = current['sha256']
    ctx['xbe_comparison_sha256'] = receipt['sha256']
    ctx['xbe_comparison_path'] = str(comparison_path)
    ctx['artifact_mode'] = mode
    ctx['artifact_origin'] = artifact_origin
    ctx['xbe_branch'] = branch
    ctx['timed_artifact_observations'] = [xbe_observation, map_observation]
    return result


def run_one_build(c, ctx):
    """Called once by the trusted parent runner after complete receipt preflight."""
    from xemu_host import SessionLock
    from xemu_session_guard import require_no_project_xemu
    lock = SessionLock(ROOT)
    lock.acquire()
    try:
        require_no_project_xemu(ROOT)
        run_one_build_locked(c, ctx, lock.inherited_fds())
        require_no_project_xemu(ROOT)
    finally:
        lock.close()


def run_one_build_locked(c, ctx, inherited_fds):
    """Keep native exclusion through the sole wrapper and final raw snapshots."""
    contract = ctx['contract']
    c.require_hourly_ownership(contract)
    require(c.datetime.datetime.now(c.datetime.timezone.utc) >=
            c.datetime.datetime.fromisoformat(contract['batch']['not_before_utc']), 'Before approved hourly slot')
    c.validate_sources(contract)
    environment = build_environment()
    make_resolution()
    tools = tool_records(c, contract)
    origin, origin_records, unused_proof = actual09_origin(c, contract)
    ctx['protected_records'].extend(origin_records)
    before_dependencies = dependency_records(c)
    before_xbe, xbe_record, before_xbe_observation = snapshot(c, ROOT / 'build/xbox/disc/default.xbe',
                                                            XBE_BYTES, with_observation=True)
    before_map, map_record, before_map_observation = snapshot(c, ROOT / 'build/xbox/main.map',
                                                            MAP_BYTES, with_observation=True)
    declared_observation(c, before_xbe_observation, xbe_record)
    declared_observation(c, before_map_observation, map_record)
    paths = contract['paths']
    for name in ('build_start', 'build_manifest', 'build_log', 'consumer_out'):
        require(not os.path.lexists(paths[name]), 'Prior build target exists: ' + name)
    require(not os.path.lexists(REFERENCE / 'consumer-attempt.json'), 'Consumer already consumed')
    fd = os.open(paths['build_log'], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    command = ['/usr/bin/bash', '--noprofile', '--norc', 'tools/build-xbox.sh']
    pid, driver_pid = os.getppid(), os.getpid()
    start = dict(kind='RF_SAVED_CONSUMER_BUILD_START_V1', batch=contract['batch']['id'],
        source_commit=contract['consumer_source_commit'], source_tree=contract['consumer_source_tree'],
        parent_runner_pid=pid, parent_runner_start_ticks=parent_identity(pid),
        build_driver_pid=driver_pid, build_driver_start_ticks=parent_identity(driver_pid),
        continuation_contract_sha256=ctx['contract_sha256'], policy_sha256=POLICY_SHA,
        tool_records=tools, before_xbe=xbe_record, before_map=map_record,
        before_xbe_observation=before_xbe_observation, before_map_observation=before_map_observation,
        actual09_reuse_policy_sha256=REUSE_POLICY_SHA, actual09_origin=origin,
        before_dependencies=dependency_identity(c, before_dependencies),
        command=command, build_environment=environment,
        wall_start_ns=time.time_ns(), monotonic_start_ns=time.monotonic_ns())
    try:
        start_record = exclusive_json(c, paths['build_start'], start)
        c.require_hourly_ownership(contract)
        # Exactly one ordinary wrapper invocation, no command-line make override.
        outcome = subprocess.run(command, cwd=ROOT, env=environment, stdout=fd,
                                 stderr=subprocess.STDOUT, pass_fds=inherited_fds)
        end_wall, end_mono = time.time_ns(), time.monotonic_ns()
        os.fsync(fd)
        log_stat = os.fstat(fd)
    finally:
        os.close(fd)
    require(outcome.returncode == 0, 'Single Xbox build failed; no retry permitted')
    require(pid == os.getppid() and start['parent_runner_start_ticks'] == parent_identity(pid), 'Build parent changed')
    c.validate_sources(contract)
    require(c.same_json(tools, tool_records(c, contract)), 'Selected builder/source changed during build')
    dependencies = dependency_records(c)
    c.require(c.unchanged(ctx['protected_records']), 'Pre-build approved source/receipt inputs changed')
    current_xbe, xbe_record, xbe_observation = snapshot(c, ROOT / 'build/xbox/disc/default.xbe',
                                                      XBE_BYTES, with_observation=True)
    current_map, map_record, map_observation = snapshot(c, ROOT / 'build/xbox/main.map',
                                                      MAP_BYTES, with_observation=True)
    declared_observation(c, xbe_observation, xbe_record)
    declared_observation(c, map_observation, map_record)
    log_data, log_record = snapshot(c, paths['build_log'], 64 * 1024**2)
    require((log_stat.st_dev, log_stat.st_ino, log_stat.st_size) ==
            (log_record['device'], log_record['inode'], log_record['bytes']), 'Build log ownership changed')
    cxbe_lines = [line for line in log_data.splitlines() if b'[ CXBE' in line]
    exact_line = ('[ CXBE     ] ' + str(ROOT / 'build/xbox/disc/default.xbe')).encode('ascii')
    require(cxbe_lines in ([], [exact_line]), 'Unexpected/multiple CXBE steps; no retry')
    interval = dict(wall_start_ns=start['wall_start_ns'], wall_end_ns=end_wall,
        monotonic_start_ns=start['monotonic_start_ns'], monotonic_end_ns=end_mono,
        builder_path=str(SDK / 'tools/cxbe/cxbe'), builder_sha256=EXTERNAL_PINS[str(SDK / 'tools/cxbe/cxbe')],
        policy_sha256=POLICY_SHA, tool_records=tools, cxbe_steps=len(cxbe_lines),
        build_driver_pid=driver_pid, build_driver_start_ticks=start['build_driver_start_ticks'],
        dependencies=dependency_identity(c, dependencies), actual09_reuse_policy_sha256=REUSE_POLICY_SHA)
    mode, artifact_origin = artifact_identity(c, contract, start, xbe_observation,
                                             map_observation, len(cxbe_lines), origin)
    build = dict(batch=contract['batch']['id'], source_tree=contract['consumer_source_tree'],
        parent_runner_pid=pid, parent_runner_start_ticks=start['parent_runner_start_ticks'],
        stock_profile=0, status='PASS_XBOX_BUILD', source_commit=contract['consumer_source_commit'],
        xbe=xbe_record['sha256'], map=map_record['sha256'], runtime='PENDING_VERIFIED_SAVED_CONSUMER_ONLY',
        build_start_sha256=start_record['sha256'], producing_build=interval,
        xbe_record=xbe_record, map_record=map_record, build_log_record=log_record)
    build.update(artifact_mode=mode, artifact_origin=artifact_origin,
                 xbe_observation=xbe_observation, map_observation=map_observation)
    exclusive_json(c, paths['build_manifest'], build)
    print('PASS_SINGLE_XBOX_BUILD_RAW_IDENTITIES_RETAINED', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--one-build', action='store_true', required=True)
    for name in ('contract', 'contract-sha256', 'approval', 'approval-sha256', 'receipt', 'receipt-sha256'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT / 'tools'))
    import xemu_natural_save_continuation as c
    ctx = c.load_context(ROOT, args.contract, args.contract_sha256, args.approval,
                         args.approval_sha256, args.receipt, args.receipt_sha256)
    run_one_build(c, ctx)


if __name__ == '__main__':
    main()
