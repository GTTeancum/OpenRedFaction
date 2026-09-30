"""Bounded Xbox-only ordinary save/reload or read-only fixture restore.

No PC executable, screenshot, host input, campaign route, or user HDD is used.
The default saves an authored L1S2 spawn on the owned test HDD, then reloads it.
Jeep exit mode checks a restored L12S1 driver with one process-local Use edge.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time

from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_smoke import Monitor
from xemu_world_hdd import prepare


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
EMULATOR = Path('C:/Games/Emulators/Xemu')
FLAGS = (
    'campaign-spawn.flag', 'campaign-level.bin', 'campaign-actor.bin',
    'campaign-passive-roof.bin', 'campaign-single-fire.bin',
    'campaign-item.bin', 'campaign-exit.bin', 'campaign-return.bin',
    'campaign-setup.bin', 'campaign-setup-immediate.flag', 'campaign-disable-auto.bin', 'campaign-goal.bin', 'campaign-goto.bin',
    'campaign-npc-drop.bin',
    'campaign-exit-start.bin', 'campaign-trigger-start.bin',
    'campaign-quick-actions.bin', 'player-replay.bin',
    'player-control-frames.txt', 'dev-room.flag', 'vehicle-test.flag',
    'dev-npc.flag', 'firearms-test.flag', 'fusion-test.flag',
    'world-hdd-save.flag', 'world-hdd-load.flag',
    'world-fixture-load.flag', 'world-fixture.0', 'world-fixture.1',
)


def build(run, name):
    with (run / f'{name}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'),
                       stdout=log, stderr=subprocess.STDOUT, check=True)


def address(mapping, name):
    match = re.search(r'_' + re.escape(name) + r'\s+([0-9a-fA-F]+)', mapping)
    if not match:
        raise RuntimeError('Missing Xbox symbol ' + name)
    return int(match.group(1), 16)


def run_guest(run, name, hdd, frames, seconds, snapshot=False, extra_symbols=None,
              allow_guest_error=False, allow_player_dead=False, capture_world=False,
              probe=None, probe_frame=None):
    phase_dir = run / name
    phase_dir.mkdir()
    shutil.copyfile(EMULATOR / 'eeprom.bin', phase_dir / 'eeprom.bin')
    config = phase_dir / 'xemu.toml'
    config.write_text(f'''[general]
show_welcome = false
skip_boot_anim = true
[general.updates]
check = false
[input]
auto_bind = false
background_input_capture = false
[net]
enable = false
[audio]
use_dsp = true
[sys.files]
bootrom_path = '{(EMULATOR / 'MCPX/mcpx_1.0.bin').as_posix()}'
flashrom_path = '{(EMULATOR / 'BIOS/xbox-4627_debug.bin').as_posix()}'
eeprom_path = '{(phase_dir / 'eeprom.bin').as_posix()}'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{(ROOT / 'build/xbox/redfaction-diagnostic.iso').as_posix()}'
''')
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    command = [str(EMULATOR / 'xemu.exe'), '-config_path', str(config),
               '-m', '64', '-display', 'xemu', '-audio', 'none',
               '-qmp', f'tcp:127.0.0.1:{port},server=on,wait=off']
    if snapshot:
        command.append('-snapshot')
    mapping = (ROOT / 'build/xbox/main.map').read_text()
    symbols = {key: address(mapping, key) for key in
               ('rf_diagnostic', 'rf_scene_world_checkpoint_state',
                'rf_xbox_checkpoint_storage_state', 'rf_scene_player_life',
                'rf_scene_checkpoint_world_route_contact',
                'rf_xbox_level_transitions', 'rf_player_replay_diagnostic',
                'rf_scene_actor_frame_count', 'rf_scene_campaign_load_stage',
                'rf_scene_follow_level_exits', 'rf_scene_level_transition')}
    if capture_world:
        symbols['rf_scene_world_checkpoint_data'] = address(mapping, 'rf_scene_world_checkpoint_data')
    extra_symbols = extra_symbols or {}
    extra_addresses = {key: address(mapping, key) for key in extra_symbols}
    monitor = process = None
    interim_probe = None
    try:
        require_no_project_xemu(ROOT)
        startup = None
        if os.name == 'nt':
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (phase_dir / 'stdout.log').open('wb') as out, (phase_dir / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=phase_dir,
                                       env=dict(os.environ, SDL_AUDIO_DRIVER='dummy'),
                                       stdout=out, stderr=err, startupinfo=startup,
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            deadline = time.monotonic() + seconds
            last = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'{name}: XEMU exited {process.returncode}')
                if monitor is None:
                    try:
                        monitor = Monitor(port)
                    except OSError:
                        time.sleep(.5)
                        continue
                    memory = monitor.command('query-memory-size-summary')
                    if memory.get('base-memory') != 64 * 1024 * 1024:
                        raise RuntimeError(f'{name}: not stock 64 MiB')
                try:
                    diagnostic = words(monitor, symbols['rf_diagnostic'], 58)
                except RuntimeError as exc:
                    if 'received 0' not in str(exc):
                        raise
                    time.sleep(.5)
                    continue
                if diagnostic[0] != 0x52464447:
                    time.sleep(.5)
                    continue
                if probe and probe_frame is not None and interim_probe is None and \
                   diagnostic[37] >= probe_frame and diagnostic[2] == 2:
                    monitor.command('stop')
                    try:
                        interim_probe = probe(monitor, mapping)
                    finally:
                        monitor.command('cont')
                stage = (diagnostic[2], diagnostic[37] // 30)
                if stage != last:
                    print(f'{name}: phase {stage[0]}, frame {diagnostic[37]}', flush=True)
                    last = stage
                if diagnostic[2] == 5 or diagnostic[2] & 0x80000000:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError(f'{name}: XEMU did not finish in {seconds}s')
            monitor.command('stop')
            state = words(monitor, symbols['rf_scene_world_checkpoint_state'], 10)
            storage = words(monitor, symbols['rf_xbox_checkpoint_storage_state'], 8)
            life = words(monitor, symbols['rf_scene_player_life'], 8)
            route_contact = words(monitor, symbols['rf_scene_checkpoint_world_route_contact'], 2)
            transitions = words(monitor, symbols['rf_xbox_level_transitions'], 4)
            replay_state = words(monitor, symbols['rf_player_replay_diagnostic'], 4)
            actor_frame_count = words(monitor, symbols['rf_scene_actor_frame_count'], 1)[0]
            load_stage = words(monitor, symbols['rf_scene_campaign_load_stage'], 1)[0]
            follow_exits = words(monitor, symbols['rf_scene_follow_level_exits'], 1)[0]
            level_request = words(monitor, symbols['rf_scene_level_transition'], 20)
            result = dict(memory_bytes=64 * 1024 * 1024, frames=diagnostic[37],
                          guest_phase=diagnostic[2],
                          free_pages=diagnostic[44], checkpoint_state=state,
                          storage_state=storage, player_life=life,
                          level_transitions=transitions,
                          replay_state=replay_state, actor_frame_count=actor_frame_count,
                          campaign_load_stage=load_stage, follow_exits=follow_exits,
                          level_request=level_request,
                          shallow_contacts=dict(npc=route_contact[0], player=route_contact[1]))
            if extra_addresses:
                result['extra'] = {key: words(monitor, extra_addresses[key], count)
                                   for key, count in extra_symbols.items()}
            if probe:
                if probe_frame is not None and interim_probe is None:
                    raise RuntimeError(f'{name}: missed live probe frame {probe_frame}')
                result['probe'] = interim_probe if probe_frame is not None else probe(monitor, mapping)
            if capture_world and not (diagnostic[2] & 0x80000000):
                pointer = words(monitor, symbols['rf_scene_world_checkpoint_data'], 1)[0]
                if state[3] != 0 or not 320 <= state[4] <= 110524 or not pointer:
                    raise RuntimeError(f'{name}: Xbox world checkpoint unavailable: {state}')
                data = bytearray()
                for offset in range(0, state[4], 4096):
                    count = (min(4096, state[4] - offset) + 3) // 4
                    data.extend(struct.pack('<' + 'I' * count,
                                            *words(monitor, pointer + offset, count)))
                payload = bytes(data[:state[4]])
                (phase_dir / 'xbox-world.rfwc').write_bytes(payload)
                result['world_checkpoint_sha256'] = hashlib.sha256(payload).hexdigest()
            (phase_dir / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            if diagnostic[2] & 0x80000000 and not allow_guest_error:
                raise RuntimeError(f'{name}: guest error {diagnostic[2]:08x}, '
                                   f'world checkpoint {state}, '
                                   f'NPC projectile telemetry {result.get("extra")}')
            if allow_guest_error and diagnostic[2] & 0x80000000:
                return result
            if diagnostic[37] != frames:
                raise RuntimeError(f'{name}: expected {frames} frames, got {diagnostic[37]}; '
                                   f'level transitions {transitions}, player life {life}, '
                                   f'replay {replay_state}, actor frames {actor_frame_count}, '
                                   f'load stage {load_stage}, follow {follow_exits}, '
                                   f'level request {level_request}')
            if not 0 < result['free_pages'] <= 16384 or (life[2] and not allow_player_dead):
                raise RuntimeError(f'{name}: memory exhausted or player dead')
            return result
    finally:
        if monitor:
            try:
                monitor.command('quit')
            except (OSError, RuntimeError):
                pass
            monitor.close()
        if process:
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=64)
    parser.add_argument('--seconds', type=int, default=180)
    parser.add_argument('--fixture', type=Path,
                        help='Read-only native load from an existing RFSG .0/.1 file on the test disc')
    parser.add_argument('--fixture-mode', choices=('shallow', 'jeep-exit'), default='shallow')
    parser.add_argument('--setup-uid', type=int,
                        help='Fire one authored L1S2 event at frame zero before ordinary save')
    parser.add_argument('--setup-source-uid', type=int,
                        help='Live authored event UID passed as source to delayed Message setup')
    parser.add_argument('--setup-actor-uid', type=int,
                        help='Live authored event UID passed as actor to delayed Message setup')
    args = parser.parse_args()
    if not 32 <= args.frames <= 600 or not 30 <= args.seconds <= 3600:
        parser.error('Require 32..600 frames and 30..3600 seconds')
    fixture_data = None
    if args.fixture:
        if args.fixture.suffix not in ('.0', '.1') or not args.fixture.is_file():
            parser.error('Fixture must be an existing RFSG .0 or .1 file')
        fixture_data = args.fixture.read_bytes()
        if len(fixture_data) < 24 or fixture_data[:4] != b'RFSG' or \
           struct.unpack_from('<I', fixture_data, 4)[0] != 1 or \
           len(fixture_data) != 24 + struct.unpack_from('<I', fixture_data, 12)[0]:
            parser.error('Fixture has an invalid RFSG header or payload length')
    elif args.fixture_mode != 'shallow':
        parser.error('--fixture-mode requires --fixture')
    if args.setup_uid is not None and (fixture_data is not None or args.setup_uid <= 0):
        parser.error('--setup-uid requires ordinary L1S2 save mode and a positive UID')
    if (args.setup_source_uid is None) != (args.setup_actor_uid is None) or \
       (args.setup_source_uid is not None and
        (args.setup_uid is None or args.setup_source_uid <= 0 or args.setup_actor_uid <= 0)):
        parser.error('Source and actor UIDs must both be positive and require --setup-uid')
    require_no_project_xemu(ROOT)
    base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not base.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    hdd = base if fixture_data is not None else prepare(ROOT, base)
    run = ROOT / 'artifacts/xemu' / ('native-world-' +
          datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    flag_names = set(FLAGS) | {path.name for path in DISC.glob('campaign-*') if path.is_file()}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(flag_names)}
    (run / 'disc-restore.json').write_text(json.dumps({
        name: value.hex() if value is not None else None
        for name, value in original.items()}, indent=2) + '\n')
    report = dict(result='FAIL', scope=('Xbox-only ordinary L12S1 Jeep post-load exit'
                  if args.fixture_mode == 'jeep-exit' else
                  'Xbox-only ordinary L1S2 fixture restore' if fixture_data is not None
                  else 'Xbox-only ordinary L1S2 pending event save/reload' if args.setup_uid
                  else 'Xbox-only ordinary L1S2 spawn save/reload'),
                  hdd=str(hdd), frames=args.frames, phases={})
    try:
        for name in original:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            (b'levels2.vpp' if args.fixture_mode == 'jeep-exit' else b'levels1.vpp').ljust(64, b'\0') +
            (b'L12S1.rfl' if args.fixture_mode == 'jeep-exit' else b'L1S2.rfl').ljust(64, b'\0'))
        if args.fixture_mode == 'jeep-exit':
            rows = [struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0,
                                int(frame == 20), 0, 0, 0, 0)
                    for frame in range(args.frames)]
            (DISC / 'player-replay.bin').write_bytes(
                b'RFI6' + struct.pack('<I', 48) + b''.join(rows))
        else:
            (DISC / 'player-replay.bin').write_bytes(
                b'RFI5' + struct.pack('<I', 44) + bytes(args.frames * 44))
        if args.setup_uid is not None:
            setup = (args.setup_uid,) if args.setup_source_uid is None else \
                    (args.setup_uid, args.setup_source_uid, args.setup_actor_uid)
            (DISC / 'campaign-setup.bin').write_bytes(struct.pack('<' + 'I' * len(setup), *setup))
        if fixture_data is not None:
            (DISC / 'world-fixture-load.flag').write_bytes(b'1')
            (DISC / ('world-fixture' + args.fixture.suffix)).write_bytes(fixture_data)
            report['fixture'] = dict(source=str(args.fixture.resolve()),
                                     sha256=hashlib.sha256(fixture_data).hexdigest(),
                                     payload_bytes=len(fixture_data)-24)
            build(run, 'fixture')
            loaded = run_guest(run, 'fixture', hdd, args.frames, args.seconds,
                               snapshot=True, extra_symbols=(
                                   {'rf_scene_vehicle_state': 16,
                                    'rf_scene_jeep_seats': 8}
                                   if args.fixture_mode == 'jeep-exit' else None))
            report['phases']['fixture'] = loaded
            state = loaded['checkpoint_state']
            if state[8] != 1 or state[0] != 0 or state[1] != len(fixture_data)-24:
                raise RuntimeError('Xbox optical ordinary fixture restore failed')
            if args.fixture_mode == 'jeep-exit':
                vehicle = loaded['extra']['rf_scene_vehicle_state']
                seats = loaded['extra']['rf_scene_jeep_seats']
                if vehicle[2] != 1 or vehicle[3] != 0 or vehicle[5] != 0 or seats[4] != 0:
                    raise RuntimeError(f'Xbox restored Jeep did not exit cleanly: {vehicle}, {seats}')
            elif loaded['shallow_contacts']['player'] < 1:
                raise RuntimeError('Xbox fixture did not exercise the player shallow-contact rule')
        else:
            (DISC / 'world-hdd-save.flag').write_bytes(b'1')
            build(run, 'save')
            saved = run_guest(run, 'save', hdd, args.frames, args.seconds,
                              capture_world=args.setup_uid is not None)
            report['phases']['save'] = saved
            save_state = saved['checkpoint_state']
            if save_state[9] != 1 or save_state[3] != 0 or not 320 <= save_state[4] <= 110524:
                raise RuntimeError('Xbox ordinary save failed')
            if args.setup_uid is not None:
                payload = (run / 'save/xbox-world.rfwc').read_bytes()
                section = 128 + (15 - 1) * 12
                offset, length = struct.unpack_from('<II', payload, section + 4)
                history = payload[offset:offset + length]
                if len(history) != length or history[:4] != b'RFCH' or \
                   struct.unpack_from('<I', history, 4)[0] != 4:
                    raise RuntimeError('Xbox save lacks RFCH4 section history')
                switch_levels, switch_count, event_levels, event_count = \
                    struct.unpack_from('<4I', history, 48)
                rows = 96 + switch_levels * 64 + switch_count * 32 + event_levels * 64
                matched = [struct.unpack_from('<14I', history, rows + i * 56)
                           for i in range(event_count)
                           if struct.unpack_from('<I', history, rows + i * 56 + 4)[0] == args.setup_uid]
                if len(matched) != 1 or not 0 < matched[0][8] < 10000:
                    raise RuntimeError(f'Xbox pending event was not saved in RFCH4: {matched}')
                if args.setup_source_uid is not None and \
                   (matched[0][10:14] != (2, args.setup_source_uid, 2, args.setup_actor_uid)):
                    raise RuntimeError(f'Xbox pending references were not UID mapped: {matched}')
                report['pending_event_history'] = dict(uid=args.setup_uid,
                    remaining_ms=matched[0][8], mode=matched[0][9],
                    source_kind=matched[0][10], source_uid=matched[0][11],
                    actor_kind=matched[0][12], actor_uid=matched[0][13])
            (DISC / 'world-hdd-save.flag').unlink()
            (DISC / 'world-hdd-load.flag').write_bytes(b'1')
            build(run, 'load')
            loaded = run_guest(run, 'load', hdd, args.frames, args.seconds)
            report['phases']['load'] = loaded
            load_state = loaded['checkpoint_state']
            if load_state[8] != 1 or load_state[0] != 0 or load_state[1] != save_state[4]:
                raise RuntimeError('Xbox ordinary reload failed or loaded a different payload')
            if loaded['storage_state'][4:7] != saved['storage_state'][4:7]:
                raise RuntimeError('Xbox ordinary reload selected a different generation or slot')
        report['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(run, 'restore')
        report['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not report['disc_restored']:
            report['result'] = 'FAIL'
        (run / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(run, report['result'], flush=True)
        if not report['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()
