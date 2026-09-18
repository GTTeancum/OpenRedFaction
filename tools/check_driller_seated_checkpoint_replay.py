"""Prepare or run process-local seated Driller save, resume and safe exit.

No host input or UI control. Default prepares only; --run executes three
bounded PC replays. State checks do not establish visual or audio correctness.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def checkpoint(path, occupied):
    data = path.read_bytes()
    magic, version, total, vc_size, profile, pl_size, terrain, remote = struct.unpack_from('<4s7I', data)
    assert (magic, version, total, vc_size, pl_size) == (b'RFCP', 3, len(data), 128, 544)
    offset = 32 + pl_size + terrain + remote
    assert offset + 128 == len(data)
    assert data[32:36] == b'RFPL' and data[offset:offset+4] == b'RFVC'
    player_version, player_bytes, mode = struct.unpack_from('<3I', data, 36)
    assert player_bytes == 544
    if occupied:
        assert (player_version, mode) == (3, 1), 'Expected paired seated player'
    else:
        assert player_version in (1, 2) and mode == 0, 'Exit must restore standing player format'
    blob = data[offset:]; checksum = 2166136261
    for i, value in enumerate(blob):
        checksum = ((checksum ^ (0 if 12 <= i < 16 else value)) * 16777619) & 0xffffffff
    assert struct.unpack_from('<I', blob, 12)[0] == checksum
    flags = struct.unpack_from('<I', blob, 20)[0]
    assert flags == (3 if occupied else 1), 'Vehicle occupancy did not follow save/exit'
    return {'player_version': player_version, 'vehicle_flags': flags,
            'health': struct.unpack_from('<f', blob, 24)[0],
            'position': struct.unpack_from('<3f', blob, 32),
            'cuts': struct.unpack_from('<I', blob, 104)[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'artifacts/driller-seated-checkpoint-replay')
    parser.add_argument('--exe', type=Path, default=ROOT / 'build/pc/Release/rf_pc_play.exe')
    parser.add_argument('--run', action='store_true')
    args = parser.parse_args(); folder = args.output_dir.resolve(); folder.mkdir(parents=True, exist_ok=True)
    common = {'RF_REPLAY_LEVEL': 'ctf06.rfl', 'RF_REPLAY_ARCHIVE': 'levelsm.vpp',
              'RF_REPLAY_DEV_ROOM': '1', 'RF_REPLAY_VEHICLE': '1', 'RF_REPLAY_PLAYER_CHECKPOINT': '1'}
    jobs = []
    for name, frames, parent in [('saved', 220, None), ('resumed', 60, 'saved'), ('exited', 120, 'resumed')]:
        base = folder / name; rows = []
        for frame in range(frames):
            use = name != 'resumed' and frame == 20
            forward = float(name == 'saved' and 60 <= frame < 180)
            rows.append(struct.pack('<5f7I', 0, 0, forward, 0, 0, 0, 0, int(use), 0, 0, 0, 0))
        base.with_suffix('.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + b''.join(rows))
        env = dict(common, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
        if parent:
            env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(folder / (parent + '.rfcp'))
        jobs.append({'name': name, 'frames': frames, 'env': env,
                     'command': [str(args.exe.resolve()), '--spawn-replay', str(ROOT / 'Installed_Game'),
                                 str(base.with_suffix('.bin')), str(base.with_suffix('.ppm'))]})
    report = {'status': 'PREPARED_NOT_RUN', 'cwd': str(ROOT), 'jobs': jobs,
              'scope': 'Seated RFPL3/RFVC save/resume then safe exit; visuals/audio unverified'}
    (folder / 'recipe.json').write_text(json.dumps(report, indent=2) + '\n')
    if not args.run:
        print(json.dumps(report, indent=2)); return
    clean = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
    states = {}
    for job in jobs:
        name = job['name']; output = folder / (name + '.rfcp'); output.unlink(missing_ok=True)
        log_path = folder / (name + '.log')
        with log_path.open('wb') as log:
            result = subprocess.run(job['command'], cwd=ROOT, env=dict(clean, **job['env']),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
        assert result.returncode == 0 and output.exists(), f'{name} failed; inspect {log_path}'
        states[name] = checkpoint(output, name != 'exited')
        text = log_path.read_text(errors='replace')
        counters = [list(map(int, line.split()[1:])) for line in text.splitlines() if line.startswith('VEHICLE ')]
        assert counters and len(counters[-1]) == 16
        values = counters[-1]; states[name]['runtime'] = values
        assert values[5] == 0, 'Vehicle runtime error'
        if name == 'saved':
            assert values[1] == 1 and values[2] == 0 and values[3] == 1
        else:
            assert 'PLAYER_CHECKPOINT_LOAD ' in text
            assert values[3] == int(name == 'resumed')
            assert values[2] == int(name == 'exited'), 'Expected exactly one validated exit'
    assert states['saved']['health'] == states['resumed']['health'] == states['exited']['health']
    assert states['saved']['cuts'] == states['resumed']['cuts'] == states['exited']['cuts']
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED', states=states)
    (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
