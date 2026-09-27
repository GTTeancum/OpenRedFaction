"""Xbox-only bounded death/restart check; no PC run, images or host input."""
import datetime
import json
import os
from pathlib import Path
import struct
import subprocess

from xemu_native_world_save import FLAGS, run_guest
from xemu_session_guard import require_no_project_xemu


ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
FRAMES = 1500


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                        'tools/build-xbox.sh', '--repack'], cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'), stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def main():
    require_no_project_xemu(ROOT)
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing isolated XEMU test HDD base')
    replay = (ROOT / 'artifacts/player-life/resume.bin').read_bytes()
    if replay[:8] != b'RFI4' + struct.pack('<I', 40) or len(replay) != 8 + FRAMES * 40:
        raise RuntimeError('Death/restart input fixture has changed')
    folder = ROOT / 'artifacts/xemu' / ('death-restart-' +
              datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*')}
    names |= {'campaign-spawn.flag', 'campaign-level.bin', 'campaign-actor.bin',
              'player-control.flag', 'player-control-frames.txt', 'player-replay.bin'}
    original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None
                for name in sorted(names)}
    result = {'result': 'FAIL', 'scope': 'Stock-64-MiB Xbox death-to-full-level-restart'}
    try:
        for name in names:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(
            b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        (DISC / 'campaign-actor.bin').write_bytes(struct.pack('<I', 8456))
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(replay)
        build(folder, 'run')
        listing = subprocess.run(
            ['C:/nxdk/tools/extract-xiso/build/extract-xiso.exe', '-l',
             str(ROOT / 'build/xbox/redfaction-diagnostic.iso')],
            capture_output=True, text=True, check=True).stdout
        (folder / 'disc-list.txt').write_text(listing)
        for name in ('campaign-spawn.flag', 'campaign-level.bin',
                     'player-control.flag', 'player-replay.bin'):
            if name not in listing:
                raise RuntimeError('Xbox disc is missing ' + name)
        guest = run_guest(folder, 'run', hdd, FRAMES, 360, snapshot=True)
        result['guest'] = guest
        if guest['level_transitions'][0] != 1 or guest['level_transitions'][1] != 0xfffffffc:
            raise RuntimeError('Xbox did not perform exactly one fresh level restart')
        if guest['player_life'][2]:
            raise RuntimeError('Xbox player remained dead after the restart')
        result['result'] = 'PASS'
    finally:
        for name, data in original.items():
            if data is None:
                (DISC / name).unlink(missing_ok=True)
            else:
                (DISC / name).write_bytes(data)
        build(folder, 'restore')
        result['disc_restored'] = all(
            ((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
            for name, data in original.items())
        if not result['disc_restored']:
            result['result'] = 'FAIL'
        (folder / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        print(folder, result['result'], flush=True)
        if not result['disc_restored']:
            raise RuntimeError('Xbox test disc flags were not restored')


if __name__ == '__main__':
    main()
