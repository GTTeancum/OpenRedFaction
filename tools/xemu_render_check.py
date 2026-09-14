"""Short stock64MiB render run: native framebuffer/profiles and PC gameplay checks.

Only process-local guest replay and QMP; no host input or desktop capture.
This is a renderer smoke check, not the complete campaign/parity suite.
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
import sys
import time

from PIL import Image
from xemu_guest_snapshot import words
from xemu_smoke import Monitor


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=180)
    parser.add_argument('--seconds', type=int, default=180)
    parser.add_argument('--actor', type=int, default=9858)
    parser.add_argument('--visible', action='store_true')
    args = parser.parse_args()
    if not 32 <= args.frames <= 3600 or not 30 <= args.seconds <= 600 or not 0 < args.actor < 0xffffffff:
        parser.error('Require32..3600 frames,30..600 seconds and a positive actor UID')
    root = Path(__file__).resolve().parents[1]
    emulator = Path('C:/Games/Emulators/Xemu')
    disc = root / 'build/xbox/disc'
    run = root / 'artifacts/xemu' / ('render-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    print('Run:', run, flush=True)
    report = dict(result='FAIL', frames=args.frames, actor=args.actor,
        scope='Staged L1S1 inspection camera, neutral process-local commands, native framebuffer, '
              'phase timings and selected PC gameplay-state checks. No full campaign/parity claim.', samples=[])
    payload = b'RFI5' + struct.pack('<I', 44) + bytes(args.frames * 44)
    (run / 'inputs.bin').write_bytes(payload)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_ACTOR_UID=str(args.actor))
    pc = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(run / 'inputs.bin'), str(run / 'pc-final.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    (run / 'pc-reference.txt').write_text(pc.stdout + pc.stderr)
    pc.check_returncode()
    report['pc_sha256'] = hashlib.sha256((root / 'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest()
    saved = {p.name: p.read_bytes() for p in disc.glob('campaign-*') if p.is_file()}
    for name in ('player-replay.bin', 'player-control-frames.txt', 'audio-output.flag', 'particle-step-fixtures.bin'):
        p = disc / name
        saved[name] = p.read_bytes() if p.exists() else None
    for name in ('campaign-spawn.flag', 'campaign-level.bin', 'campaign-actor.bin'):
        saved.setdefault(name, None)
    process = monitor = None
    mapping = ''

    def build():
        with (run / 'build.log').open('ab') as out:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                'tools/build-xbox.sh', '--repack'], cwd=root, env=dict(os.environ, MSYSTEM='CLANG64'),
                stdout=out, stderr=subprocess.STDOUT, check=True)

    def symbol(name):
        match = re.search('_' + name + r'\s+([0-9a-fA-F]+)', mapping)
        if not match:
            raise ValueError('Missing symbol ' + name)
        return int(match[1], 16)

    def capture(d):
        if d[32] and d[33:35] == [640, 480] and d[35] >= 2560:
            path = run / 'framebuffer.bin'
            monitor.command('human-monitor-command', {'command-line':
                f'pmemsave 0x{d[32] & 0x03ffffff:x} {d[35] * d[34]} "{path.as_posix()}"'})
            Image.frombytes('RGB', (d[33], d[34]), path.read_bytes(), 'raw', 'BGRX', d[35], 1).save(run / 'framebuffer.png')

    try:
        for name in saved:
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (disc / 'campaign-actor.bin').write_bytes(struct.pack('<I', args.actor))
        (disc / 'player-replay.bin').write_bytes(payload)
        build()
        mapping = (root / 'build/xbox/main.map').read_text()
        (run / 'main.map').write_text(mapping)
        shutil.copyfile(disc / 'default.xbe', run / 'default.xbe')
        report['xbe_sha256'] = hashlib.sha256((run / 'default.xbe').read_bytes()).hexdigest()
        hdd = root / 'local/xemu-harness/pacing-base.qcow2'
        if not hdd.exists():
            raise ValueError('Missing isolated pacing HDD base')
        shutil.copyfile(emulator / 'eeprom.bin', run / 'eeprom.bin')
        config = run / 'xemu.toml'
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
bootrom_path = '{emulator.as_posix()}/MCPX/mcpx_1.0.bin'
flashrom_path = '{emulator.as_posix()}/BIOS/xbox-4627_debug.bin'
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{root.as_posix()}/build/xbox/redfaction-diagnostic.iso'
''')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        command = [str(emulator / 'xemu.exe'), '-config_path', str(config), '-m', '64', '-snapshot',
            '-display', 'xemu', '-audio', 'none', '-qmp', f'tcp:127.0.0.1:{port},server=on,wait=off']
        report['command'] = command
        startup = None
        if os.name == 'nt' and not args.visible:
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (run / 'stdout.log').open('wb') as out, (run / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=run, env=dict(os.environ, SDL_AUDIO_DRIVER='dummy'),
                stdout=out, stderr=err, startupinfo=startup,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            report.update(pid=process.pid, port=port)
            (run / 'live.json').write_text(json.dumps(dict(pid=process.pid, port=port)))
            deadline = time.monotonic() + args.seconds
            previous = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'XEMU exited {process.returncode}')
                if monitor is None:
                    try:
                        monitor = Monitor(port)
                    except OSError:
                        time.sleep(.5)
                        continue
                    report['memory'] = monitor.command('query-memory-size-summary')
                    assert report['memory']['base-memory'] == 64 * 1024 * 1024
                try:
                    d = words(monitor, symbol('rf_diagnostic'), 58)
                except RuntimeError as exc:
                    if 'received 0' not in str(exc):
                        raise
                    time.sleep(.5)
                    continue
                if d[0] != 0x52464447:
                    time.sleep(.5)
                    continue
                report['samples'].append(dict(time=time.monotonic(), phase=d[2], frame=d[37], pages=d[44]))
                current = (d[2], d[37] // 30)
                if current != previous:
                    print('Guest phase', d[2], 'submitted', d[37], 'frames', flush=True)
                    previous = current
                if d[2] & 0x80000000:
                    raise RuntimeError(f'Guest error {d[2]:08x}')
                if d[2] == 5:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError('Bounded render run did not complete')
            monitor.command('stop')
            capture(d)
            assert d[37] == args.frames, (d[37], args.frames)
            fields = [('rf_diagnostic', 58), ('rf_xbox_retained_world', 8), ('rf_xbox_retained_models', 8),
                ('rf_xbox_retained_model_kinds', 6), ('rf_scene_pose_sharing', 4), ('rf_renderer_submission', 4), ('rf_renderer_vblank', 3)]
            fields += [(name, 32) for name in ('rf_renderer_profile', 'rf_scene_profile',
                'rf_scene_presentation_profile', 'rf_scene_world_profile', 'rf_scene_step_profile',
                'rf_scene_npc_step_profile', 'rf_scene_npc_playback_profile')]
            snap = dict(symbols={name: dict(words=words(monitor, symbol(name), count)) for name, count in fields})
            (run / 'guest-memory-final.json').write_text(json.dumps(snap, indent=2))
            report['checks'] = {}
            for name, label, count in [('scene_actor_body', 'PC_PLAY_BODY', 77),
                    ('rf_scene_player_ammo', 'PLAYER_AMMO', 8), ('rf_scene_combat', 'COMBAT', 8),
                    ('rf_scene_script_movement', 'SCRIPT_MOVE', 8), ('rf_scene_enemy_combat', 'ENEMY_COMBAT', 8)]:
                expected = list(map(int, next(line for line in pc.stdout.splitlines() if line.startswith(label + ' ')).split()[1:]))
                actual = words(monitor, symbol(name), count)
                report['checks'][label] = dict(equal=actual == expected, xbox=actual, pc=expected)
                assert actual == expected, label
            report.update(result='PASS', available_pages=d[44], diagnostic=d)
            with (run / 'performance.txt').open('w') as out:
                subprocess.run([sys.executable, 'tools/summarize_xbox_performance.py',
                    str(run / 'guest-memory-final.json'), '--out', str(run / 'performance.json')],
                    cwd=root, stdout=out, check=True)
    except Exception as exc:
        report['error'] = repr(exc)
        if monitor:
            try:
                monitor.command('stop')
                capture(words(monitor, symbol('rf_diagnostic'), 58))
                report['registers'] = monitor.command('human-monitor-command', {'command-line': 'info registers'})
                report['failure_telemetry'] = {name: words(monitor, symbol(name), count) for name, count in
                    [('rf_diagnostic', 58), ('rf_animation_progress', 4), ('rf_scene_profile_stage', 2),
                     ('rf_xbox_retained_world', 8), ('rf_xbox_retained_models', 8), ('rf_xbox_retained_model_kinds', 6)]}
            except Exception as capture_error:
                report['capture_error'] = repr(capture_error)
        raise
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
        for name, data in saved.items():
            if data is None:
                (disc / name).unlink(missing_ok=True)
            else:
                (disc / name).write_bytes(data)
        try:
            build()
        except Exception as exc:
            report.update(result='FAIL', restore_error=repr(exc))
            raise
        finally:
            (run / 'report.json').write_text(json.dumps(report, indent=2))
            print(run, report['result'], flush=True)


if __name__ == '__main__':
    main()
