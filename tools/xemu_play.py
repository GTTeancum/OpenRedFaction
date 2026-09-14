"""Launch an unlimited, controller-driven stock64MiB Xbox campaign session.

The mounted ISO is an isolated copy, so later builds cannot change this run.
This launcher never sends input, pauses, or automatically closes the emulator.
"""
import datetime
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    emulator = Path('C:/Games/Emulators/Xemu')
    disc = root / 'build/xbox/disc'
    run = root / 'artifacts/xemu' / ('play-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    saved = {p.name: p.read_bytes() for p in disc.glob('campaign-*') if p.is_file()}
    for name in ('campaign-spawn.flag', 'campaign-level.bin', 'player-replay.bin',
                 'player-control-frames.txt', 'player-control.flag', 'particle-step-fixtures.bin',
                 'renderer-cull-off.flag', 'renderer-cull-on.flag', 'renderer-batch-off.flag', 'renderer-world-off.flag'):
        p = disc / name
        saved.setdefault(name, p.read_bytes() if p.exists() else None)

    def build():
        with (run / 'build.log').open('ab') as out:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                'tools/build-xbox.sh', '--repack'], cwd=root,
                env=dict(os.environ, MSYSTEM='CLANG64'), stdout=out,
                stderr=subprocess.STDOUT, check=True)

    try:
        for name in saved:
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (disc / 'player-control.flag').write_bytes(b'')
        build()
        shutil.copyfile(root / 'build/xbox/redfaction-diagnostic.iso', run / 'game.iso')
        shutil.copyfile(root / 'build/xbox/main.map', run / 'main.map')
        shutil.copyfile(disc / 'default.xbe', run / 'default.xbe')
    finally:
        for name, data in saved.items():
            if data is None:
                (disc / name).unlink(missing_ok=True)
            else:
                (disc / name).write_bytes(data)
        build()

    hdd = root / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.exists():
        raise FileNotFoundError(hdd)
    shutil.copyfile(emulator / 'eeprom.bin', run / 'eeprom.bin')
    config = run / 'xemu.toml'
    config.write_text(f'''[general]
show_welcome = false
skip_boot_anim = true
[general.updates]
check = false
[input]
auto_bind = true
background_input_capture = false
[input.bindings]
port1_driver = 'usb-xbox-gamepad'
port1 = '0300fa675e040000ff02000000007801'
[net]
enable = false
[audio]
use_dsp = true
[sys.files]
bootrom_path = '{emulator.as_posix()}/MCPX/mcpx_1.0.bin'
flashrom_path = '{emulator.as_posix()}/BIOS/xbox-4627_debug.bin'
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{run.as_posix()}/game.iso'
''')
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    command = [str(emulator / 'xemu.exe'), '-config_path', str(config), '-m', '64',
               '-snapshot', '-display', 'xemu', '-qmp', f'tcp:127.0.0.1:{port},server=on,wait=off']
    with (run / 'stdout.log').open('wb') as out, (run / 'stderr.log').open('wb') as err:
        process = subprocess.Popen(command, cwd=run, stdout=out, stderr=err,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    (run / 'live.json').write_text(json.dumps(dict(pid=process.pid, port=port,
        frame_limit=0, controller_port=1, command=command), indent=2))
    print(f'XEMU PID {process.pid}, QMP {port}; left running without a frame limit.\n{run}', flush=True)


if __name__ == '__main__':
    main()
