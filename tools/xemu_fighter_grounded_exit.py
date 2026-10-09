"""Parent-only hourly stock-64-MiB check of the original L18S2 Fighter exit.

Run only after the parent Xbox build. This runner never compiles: it packages
the already-built exact XBE, streams ordinary guest Use/crouch/Use, and reads
native diagnostics. No edited scene, inventory grant, campaign traversal,
capture, host input or PC runtime is involved. The isolated HDD is snapshot-only.

Example (parent 04:00 UTC batch):
  python tools/xemu_fighter_grounded_exit.py --parent-hourly-batch

The original ISO is moved aside and restored byte-for-byte. All affected disc
flags are saved/restored under one SessionLock, which remains inherited by XEMU.
The exact XBE/map are copied to the compact evidence directory before launch.
"""

import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess

os.environ.setdefault('RF_XEMU_ROOT', '/workspace/shared/xemu-inputs')
os.environ.setdefault('RF_XEMU_BINARY', '/workspace/shared/xemu/squashfs-root/usr/bin/xemu')

import xemu_native_world_save as native
from xemu_guest_snapshot import words
from xemu_host import SessionLock
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import standalone


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
ISO = ROOT / 'build/xbox/redfaction-diagnostic.iso'
MAP = ROOT / 'build/xbox/main.map'
XBE = DISC / 'default.xbe'
HDD = ROOT / 'local/xemu-harness/pacing-base.qcow2'
HOST_UID = 10066
FRAMES = 640
BOARD_FRAME, EARLY_FRAME = 12, 40
DESCEND_BEGIN, DESCEND_END = 180, 520
GROUNDED_FRAME, EXIT_FRAME = 540, 600
AUTHORED_POSITION = (-13.6331787109375, 28.193084716796875, 103.06741333007812)
UINT_MAX = 0xffffffff
SYMBOLS = {
    'rf_scene_vehicle_enabled': 1,
    'rf_scene_vehicle_selection': 12,
    'rf_scene_vehicle_entry_probe': 16,
    'rf_scene_vehicle_state': 16,
    'rf_scene_vehicle_damage': 8,
    'rf_scene_vehicle_ground_probe': 20,
    'rf_scene_vehicle_exit_probe': 80,
    'rf_scene_vehicle_route_state': 8,
    'rf_scene_fighter_weapon': 8,
    'rf_scene_vehicle_cockpit_hud': 16,
    'rf_hud_vehicle_diagnostic': 8,
    'rf_hud_assets_diagnostic': 8,
    'rf_scene_enemy_combat': 8,
    'rf_scene_actor_landing': 8,
    'rf_scene_actor_ground_stats': 8,
    'rf_scene_player_life': 8,
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def floats(values):
    return list(struct.unpack('<' + 'f' * len(values), struct.pack('<' + 'I' * len(values), *values)))


def number(word):
    value = floats([word])[0]
    # Preserve nonfinite bit patterns in raw telemetry without invalid JSON.
    return value if math.isfinite(value) else None


def decoded_exit(raw):
    directions = []
    stages = {0: 'not_attempted', 1: 'started', 2: 'path_blocked', 3: 'floor_absent',
              4: 'bad_support', 5: 'host_overlap', 6: 'final_placement_blocked',
              7: 'accepted', 8: 'error', 9: 'water_blocked'}
    for direction in range(4):
        row = raw[16 + direction * 16:32 + direction * 16]
        directions.append(dict(direction=direction, stage=stages.get(row[0], 'unknown'),
            stage_code=row[0], status=row[1], candidate_xyz=[number(v) for v in row[2:5]],
            path_blocked=row[5], floor_found=row[6], floor_fraction=number(row[7]),
            floor_normal_y=number(row[8]), floor_solid=row[9], floor_room=row[10],
            final_y=number(row[11]), host_clear=row[12], final_clear=row[13],
            floor_contact_y=number(row[14]), floor_sphere=row[15]))
    return dict(attempts=raw[0], frame=raw[1], host=raw[2], accepted=raw[3],
        status=raw[4], directions_attempted=raw[5], seated_head_transit=raw[6],
        drop=number(raw[7]), seat_xyz=[number(v) for v in raw[8:11]],
        host_xyz=[number(v) for v in raw[11:14]], body_spheres=raw[14],
        body_bottom_offset=number(raw[15]), directions=directions)


def sample_callback(folder, name):
    def read(monitor, mapping):
        result = {key: words(monitor, native.address(mapping, key), count)
                  for key, count in SYMBOLS.items()}
        result['frame'] = words(monitor, native.address(mapping, 'rf_diagnostic'), 58)[37]
        result['exit_decoded'] = decoded_exit(result['rf_scene_vehicle_exit_probe'])
        write_json(folder / (name + '.json'), result)
        return result
    return read


def replay():
    rows = [struct.pack('<5f7I', 0, 0, 0, 0, 0,
                        int(DESCEND_BEGIN <= frame < DESCEND_END), 0,
                        int(frame in (BOARD_FRAME, EXIT_FRAME)), 0, 0, 0, 0)
            for frame in range(FRAMES)]
    return b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def evaluate(result):
    checks = {
        'stock_64_mib': result.get('memory_bytes') == 64 * 1024 * 1024,
        'completed_exact_replay': result.get('guest_phase') == 5 and result.get('frames') == FRAMES,
        'memory_available': 0 < result.get('free_pages', 0) <= 16384,
        'no_level_transition': result.get('level_transitions', [None])[0] == 0,
        'no_death_or_respawn': result.get('player_life', [])[:3] == [0, 0, 0],
        'both_live_samples': bool(result.get('probe')) and bool(result.get('final_probe')),
    }
    report = dict(scope='Original L18S2 UID10066, one ordinary board/descent/living exit; no fire',
        checks=checks, limits='Read-only native gameplay/HUD submission evidence, not visual parity, '
        'performance, combat, save/reload, airborne exit or campaign-route coverage.')
    early, grounded, end = result.get('probe'), result.get('final_probe'), result.get('extra')
    if early and grounded and end:
        phases = (early, grounded, end)
        vehicle = [phase['rf_scene_vehicle_state'] for phase in phases]
        positions = [floats(state[6:9]) for state in vehicle]
        life = [phase['rf_scene_player_life'] for phase in phases]
        health = [number(phase['rf_scene_enemy_combat'][5]) for phase in phases]
        ground = grounded['rf_scene_vehicle_ground_probe']
        trace = end['rf_scene_vehicle_exit_probe']
        cockpit = [phase['rf_scene_vehicle_cockpit_hud'] for phase in phases]
        assets = [phase['rf_hud_assets_diagnostic'] for phase in phases]
        hud = [phase['rf_hud_vehicle_diagnostic'] for phase in phases]
        accepted = [trace[16 + i * 16:32 + i * 16] for i in range(4)
                    if trace[16 + i * 16] == 7]
        checks.update({
            'samples_in_correct_windows': EARLY_FRAME <= early['frame'] < DESCEND_BEGIN and
                GROUNDED_FRAME <= grounded['frame'] < EXIT_FRAME,
            'original_fighter_selected': all(phase['rf_scene_vehicle_enabled'] == [5] and
                phase['rf_scene_vehicle_selection'][8:10] == [HOST_UID, 5] for phase in phases),
            'one_ordinary_board': vehicle[0][1:4] == [1, 0, 1] and
                early['rf_scene_vehicle_entry_probe'][0] == 1 and
                early['rf_scene_vehicle_entry_probe'][2:6] == [5, vehicle[0][12], 1, 0],
            'authored_hover_start': all(math.isfinite(value) and abs(value - expected) < .05
                for value, expected in zip(positions[0], AUTHORED_POSITION)),
            'real_vertical_descent': all(math.isfinite(v) for p in positions for v in p) and
                positions[0][1] - positions[1][1] > 60,
            'unchanged_horizontal_host_pose': all(math.hypot(p[0] - AUTHORED_POSITION[0],
                p[2] - AUTHORED_POSITION[2]) < .05 for p in positions),
            'still_occupied_on_ground': vehicle[1][1:4] == [1, 0, 1],
            'actual_downward_world_or_mover_contact': ground[0] > 0 and
                DESCEND_BEGIN <= ground[1] < EXIT_FRAME and
                (ground[2] != UINT_MAX or ground[3] != UINT_MAX) and
                number(ground[6]) is not None and 0 <= number(ground[6]) <= 1 and
                number(ground[8]) is not None and number(ground[8]) >= .5 and
                number(ground[14]) is not None and number(ground[17]) is not None and
                number(ground[14]) > number(ground[17]),
            'ground_contact_binds_to_fighter_pose': all(number(ground[13 + axis]) is not None and
                abs(number(ground[13 + axis]) - positions[1][axis]) < .05 for axis in range(3)),
            'descent_stopped_before_exit': number(vehicle[1][10]) is not None and
                abs(number(vehicle[1][10])) < .05,
            'one_ordinary_exit_at_600': vehicle[2][1:4] == [1, 1, 0] and
                trace[0:5] == [1, EXIT_FRAME, vehicle[0][12], 1, 0],
            'accepted_complete_body_support': len(accepted) == 1 and trace[14] > 0 and
                accepted[0][1] == 0 and accepted[0][5:7] == [0, 1] and
                number(accepted[0][7]) is not None and 0 <= number(accepted[0][7]) < 1 and
                number(accepted[0][8]) is not None and number(accepted[0][8]) >= .5 and
                (accepted[0][9] != UINT_MAX or accepted[0][10] != UINT_MAX) and
                accepted[0][12:14] == [1, 1],
            'living_through_exit': all(value is not None and value > 0 for value in health) and
                all(phase['rf_scene_enemy_combat'][6] == 0 for phase in phases) and
                all(state[:3] == [0, 0, 0] for state in life),
            'no_player_health_loss': health[0] == health[1] == health[2],
            'host_alive_without_damage': all(phase['rf_scene_vehicle_damage'][2:6] == [0, 0, 0, 0]
                and phase['rf_scene_vehicle_damage'][7] == 1 for phase in phases),
            'no_weapons_fired': all(phase['rf_scene_fighter_weapon'][1:6] == [0] * 5
                for phase in phases),
            'vehicle_ammunition_unchanged': all(phase['rf_scene_fighter_weapon'][6:8] ==
                early['rf_scene_fighter_weapon'][6:8] for phase in phases),
            'no_runtime_or_route_error': all(phase['rf_scene_vehicle_state'][5] == 0 and
                phase['rf_scene_vehicle_route_state'][7] == 0 and phase['rf_scene_enemy_combat'][7] == 0
                for phase in phases),
            'native_fighter_cockpit_drawn': all(state[0] == 5 and state[1] > 0 and
                state[2] > 0 and state[3] > 0 and state[13] == 0 for state in cockpit[:2]),
            'native_hud_assets_active': all(state[1] == 0 and state[2] == 1 and
                state[5] > 0 and state[6] == 0 and state[7] == 0 for state in assets),
            'ordinary_hud_returns_after_exit': hud[0][1] == hud[1][1] == 5 and
                hud[2][1:] == [0] * 7 and assets[2][5] > assets[1][5],
            'ordinary_player_ground_reacquired': end['rf_scene_actor_landing'][1] == 1 and
                end['rf_scene_actor_landing'][4] > grounded['rf_scene_actor_landing'][4],
        })
        report.update(host_positions=[[number(v) for v in state[6:9]] for state in vehicle], player_health=health,
            actual_host_ground_contact=dict(count=ground[0], frame=ground[1], solid=ground[2],
                room=ground[3], normal=[number(v) for v in ground[7:10]],
                point=[number(v) for v in ground[10:13]],
                sweep_start=[number(v) for v in ground[13:16]],
                sweep_end=[number(v) for v in ground[16:19]]),
            exit=decoded_exit(trace))
    report['failed_checks'] = [name for name, passed in checks.items() if not passed]
    report['status'] = 'PASS_BOUNDED_FIGHTER_GROUNDED_EXIT' if not report['failed_checks'] else 'CHECK_FAILED'
    return report


class BorrowedSessionLock:
    """Let the existing native runner borrow the outer staging/restore lock."""
    def __init__(self, owner):
        self.owner = owner

    def acquire(self):
        pass

    def close(self):
        pass

    def inherited_fds(self):
        return self.owner.inherited_fds()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-hourly-batch', action='store_true', required=True)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if not 120 <= args.seconds <= 1800:
        parser.error('--seconds must be in 120..1800')
    folder = args.out or ROOT / 'artifacts/xemu' / ('fighter-grounded-exit-' +
        datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'))
    folder = folder.resolve()
    if folder.exists():
        parser.error('Evidence directory already exists; use a new --out path')
    packer = Path(os.environ.get('RF_EXTRACT_XISO', '/workspace/shared/nxdk/tools/extract-xiso/build/extract-xiso'))
    for path in (XBE, MAP, HDD, packer, Path(os.environ['RF_XEMU_BINARY']),
                 native.EMULATOR / 'eeprom.bin', native.EMULATOR / 'MCPX/mcpx_1.0.bin',
                 native.EMULATOR / 'BIOS/xbox-4627_debug.bin'):
        if not path.is_file():
            raise RuntimeError('Missing required parent batch input: ' + str(path))
    standalone(HDD)
    mapping = MAP.read_text()
    for symbol in SYMBOLS:
        native.address(mapping, symbol)  # Fail before staging if the fix was not built.
    require_no_project_xemu(ROOT)
    owner = SessionLock(ROOT)
    owner.acquire()
    old_lock_factory = native.SessionLock
    original = None
    iso_moved = False
    staged = False
    report = dict(status='NOT_RUN', scope='Parent-only original L18S2 Fighter grounded exit',
        generated_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    try:
        require_no_project_xemu(ROOT)
        folder.mkdir(parents=True)
        original_scene = ROOT / 'Installed_Game/levels3.vpp'
        scene_hash = sha256(original_scene)
        if sha256(DISC / 'levels3.vpp') != scene_hash:
            raise RuntimeError('Disc levels3.vpp is not byte-identical to the original input')
        names = set(native.FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
        names |= {p.name for p in DISC.glob('*.flag') if p.is_file()}
        names |= {'player-control.flag', 'player-control-frames.txt', 'player-replay.bin',
                  'scene-preview.flag', 'scene-fixture.vpp', 'model-skin-test.bin', 'model-collision-test.bin'}
        original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                    for name in sorted(names)}
        write_json(folder / 'disc-restore.json', {name: data.hex() if data is not None else None
                   for name, data in original.items()})
        pins = {'default.xbe': sha256(XBE), 'main.map': sha256(MAP), 'levels3.vpp': scene_hash}
        shutil.copyfile(XBE, folder / 'tested-default.xbe')
        shutil.copyfile(MAP, folder / 'tested-main.map')
        report['exact_inputs'] = pins
        report['recipe'] = dict(source='Installed_Game/levels3.vpp/L18S2.rfl',
            host_uid=HOST_UID, authored_host_xyz=AUTHORED_POSITION, frames=FRAMES,
            process_local_staging='Existing campaign-actor.bin / rf_scene_stage_actor only',
            input={'Use': [BOARD_FRAME, EXIT_FRAME], 'crouch': [DESCEND_BEGIN, DESCEND_END - 1],
                   'release_before_exit': [DESCEND_END, EXIT_FRAME - 1], 'fire': [], 'other': []},
            observations={'board': EARLY_FRAME, 'ground': GROUNDED_FRAME, 'terminal': FRAMES},
            hdd=str(HDD), snapshot_only=True, scene_edits=False, inventory_grants=False,
            comparison='Same earlier Use12/crouch180..519/Use600/end640 timing; earlier fire omitted. '
                       'This is an exit-only check and not a performance comparison.')
        write_json(folder / 'recipe.json', report['recipe'])
        if ISO.exists():
            ISO.rename(folder / 'original-disc.iso')
            iso_moved = True
        staged = True
        for name in original:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels3.vpp'.ljust(64, b'\0') + b'L18S2.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', HOST_UID))
        (DISC / 'scene-preview.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay())
        with (folder / 'pack-only.log').open('wb') as log:
            subprocess.run([str(packer), '-c', str(DISC), str(ISO)], cwd=ROOT,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        if sha256(XBE) != pins['default.xbe'] or sha256(MAP) != pins['main.map']:
            raise RuntimeError('XBE/map changed after pinning; refusing emulator launch')
        native.SessionLock = lambda root: BorrowedSessionLock(owner)
        result = native.run_guest(folder, 'native', HDD, FRAMES, args.seconds,
            snapshot=True, extra_symbols=SYMBOLS, allow_guest_error=True, allow_player_dead=True,
            probe=sample_callback(folder, 'board-sample'), probe_frame=EARLY_FRAME,
            final_probe=sample_callback(folder, 'ground-sample'), final_probe_frame=GROUNDED_FRAME)
        report['native'] = result
        report.update(evaluate(result))
        if result.get('extra'):
            write_json(folder / 'exit-rejection-telemetry.json',
                       decoded_exit(result['extra']['rf_scene_vehicle_exit_probe']))
    except BaseException as exc:
        # run_guest writes terminal raw words before rejecting a missed probe.
        # Retain that evidence even when it cannot return a complete result.
        result_path = folder / 'native/result.json'
        if 'native' not in report and result_path.is_file():
            try:
                terminal = json.loads(result_path.read_text())
                report['native'] = terminal
                raw = terminal.get('extra', {}).get('rf_scene_vehicle_exit_probe')
                if raw:
                    write_json(folder / 'exit-rejection-telemetry.json', decoded_exit(raw))
            except (OSError, ValueError, KeyError, IndexError) as recovery_error:
                report['terminal_read_error'] = str(recovery_error)
        report['status'] = 'CHECK_FAILED'
        report['error'] = type(exc).__name__ + ': ' + str(exc)
        raise
    finally:
        native.SessionLock = old_lock_factory
        restoration_errors = []
        try:
            if staged and original is not None:
                for name, data in original.items():
                    try:
                        if data is None:
                            (DISC / name).unlink(missing_ok=True)
                        else:
                            (DISC / name).write_bytes(data)
                    except OSError as exc:
                        restoration_errors.append(name + ': ' + str(exc))
            if staged or iso_moved:
                try:
                    ISO.unlink(missing_ok=True)
                    if iso_moved:
                        (folder / 'original-disc.iso').rename(ISO)
                except OSError as exc:
                    restoration_errors.append('ISO: ' + str(exc))
            if original is not None:
                report['disc_flags_restored'] = all(
                    ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
                    for name, data in original.items())
            if 'exact_inputs' in report:
                report['exact_xbe_map_unchanged'] = sha256(XBE) == report['exact_inputs']['default.xbe'] and \
                    sha256(MAP) == report['exact_inputs']['main.map']
                report['original_scene_unchanged'] = sha256(DISC / 'levels3.vpp') == report['exact_inputs']['levels3.vpp']
            report['original_iso_restored'] = not staged or (ISO.exists() if iso_moved else not ISO.exists())
            report['restoration_errors'] = restoration_errors
            if restoration_errors or any(report.get(key) is False for key in
                ('disc_flags_restored', 'exact_xbe_map_unchanged', 'original_scene_unchanged', 'original_iso_restored')):
                report['status'] = 'CHECK_FAILED'
            if folder.exists():
                write_json(folder / 'verification.json', report)
            print(folder, report['status'], flush=True)
        finally:
            owner.close()
    if report['status'] != 'PASS_BOUNDED_FIGHTER_GROUNDED_EXIT':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
