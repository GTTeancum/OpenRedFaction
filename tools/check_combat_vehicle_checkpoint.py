"""Prepare bounded APC/Jeep combat checkpoint replays; --run explicitly opts in.

Uses only process-local RFI6 input. No host input, emulator or automatic build.
Live save wiring is a prerequisite, not assumed working by this generator.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def inputs(vehicle, phase):
    frames = 720 if phase == 'saved' else 60 if phase == 'resumed' else 180
    rows = []
    for frame in range(frames):
        move = pitch = yaw = 0.0
        use = fire = cycle = alternate = False
        if phase == 'saved' and vehicle == 'jeep':
            move = float(10 <= frame < 62 or 100 <= frame < 150)
            use = frame == 80
            cycle = frame == 320
            pitch = -.2 if 340 <= frame < 360 else 0
            yaw = .3 if 340 <= frame < 360 else 0
            fire = 380 <= frame < 420
        elif phase == 'saved':
            move = float(10 <= frame < 25 or 70 <= frame < 80)
            use = frame == 40
            pitch = -.2 if 80 <= frame < 100 else 0
            fire = 210 <= frame < 240
            alternate = frame == 270
        elif phase == 'exited':
            cycle = vehicle == 'jeep' and frame == 20
            use = frame == (80 if vehicle == 'jeep' else 20)
        # RFI6 move3,look2,crouch,jump,use,fire,reload,cycle,alternate.
        rows.append(struct.pack('<5f7I', 0, 0, move, pitch, yaw, 0, 0,
                                int(use), int(fire), 0, int(cycle), int(alternate)))
    return frames, b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def read_checkpoint(path, vehicle, occupied):
    data = path.read_bytes()
    magic, version, total, size, _, player_size, terrain_size, remote_size = struct.unpack_from('<4s7I', data)
    expected_profile, expected_size = (2, 128) if vehicle == 'apc' else (3, 160)
    assert (magic, version, total, size, player_size) == (b'RFCP', 3, len(data), expected_size, 544)
    offset = 32 + player_size + terrain_size + remote_size
    assert offset + size == len(data)
    blob = data[offset:]
    assert blob[:4] == b'RFVC'
    assert struct.unpack_from('<2I', blob, 4) == (expected_profile, expected_size)
    assert struct.unpack_from('<I', blob, 16)[0] == expected_profile
    checksum = 2166136261
    for i, value in enumerate(blob):
        checksum = ((checksum ^ (0 if 12 <= i < 16 else value)) * 16777619) & 0xffffffff
    assert struct.unpack_from('<I', blob, 12)[0] == checksum
    assert data[32:36] == b'RFPL'
    pl_version, pl_bytes, mode = struct.unpack_from('<3I', data, 36)
    assert pl_bytes == 544
    assert (pl_version == 3 and mode == 1) if occupied else (pl_version in (1, 2) and mode == 0)
    flags = struct.unpack_from('<I', blob, 20)[0]
    assert flags == (3 if occupied else 1)
    state = {'health': struct.unpack_from('<f', blob, 24)[0],
             'primary': struct.unpack_from('<I', blob, 104)[0],
             'aim': struct.unpack_from('<2f', blob, 112),
             'random': struct.unpack_from('<I', blob, 120)[0],
             'position': struct.unpack_from('<3f', blob, 32), 'occupied': occupied}
    assert 0 < state['health'] <= (5000 if vehicle == 'apc' else 400)
    assert 0 <= state['primary'] <= 999 and all(math.isfinite(v) for v in state['aim'])
    if vehicle == 'apc':
        state['secondary'] = struct.unpack_from('<I', blob, 108)[0]
        assert 0 <= state['secondary'] <= 15
    else:
        state['role'] = struct.unpack_from('<I', blob, 108)[0]
        state['aim_reference'] = struct.unpack_from('<9f', blob, 124)
        assert state['role'] == int(occupied), 'Expected saved gunner or exited driver role'
        assert all(math.isfinite(v) for v in state['aim_reference'])
    return state


def counters(text, name, count):
    rows = [list(map(int, line.split()[1:])) for line in text.splitlines() if line.startswith(name + ' ')]
    assert rows and len(rows[-1]) == count, f'Missing {name} diagnostics'
    return rows[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vehicle', choices=('apc', 'jeep'), required=True)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build/pc/Release/rf_pc_play.exe')
    parser.add_argument('--run', action='store_true')
    args = parser.parse_args()
    folder = (args.output_dir or ROOT / f'artifacts/{args.vehicle}-combat-checkpoint').resolve()
    folder.mkdir(parents=True, exist_ok=True)
    common = {'RF_REPLAY_LEVEL': 'ctf06.rfl', 'RF_REPLAY_ARCHIVE': 'levelsm.vpp',
              'RF_REPLAY_DEV_ROOM': '1', 'RF_REPLAY_VEHICLE': args.vehicle, 'RF_REPLAY_PLAYER_CHECKPOINT': '1'}
    jobs = []
    for name, previous in [('saved', None), ('resumed', 'saved'), ('exited', 'resumed')]:
        base = folder / name
        frames, data = inputs(args.vehicle, name)
        base.with_suffix('.bin').write_bytes(data)
        env = dict(common, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
        if previous:
            env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(folder / (previous + '.rfcp'))
        jobs.append({'name': name, 'frames': frames, 'env': env,
                     'command': [str(args.exe.resolve()), '--spawn-replay', str(ROOT / 'Installed_Game'),
                                 str(base.with_suffix('.bin')), str(base.with_suffix('.ppm'))]})
    report = {'status': 'PREPARED_NOT_RUN', 'vehicle': args.vehicle, 'cwd': str(ROOT), 'jobs': jobs,
              'scope': 'State-only save/reload/exit. Visual/audio and Xbox acceptance remain separate.',
              'schedule': ('APC approach/board/short drive/upward pitch80..99;primary210..239,alternate270;neutral through719' if args.vehicle == 'apc' else
                           'Jeep established approach/board/drive;gunner320,look340..359,fire380..419;neutral through719'),
              'prerequisite': 'Matching live RFCP vehicle profile wiring; rejects are failures, not silently skipped checks.'}
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
                                    stdout=log, stderr=subprocess.STDOUT, timeout=240)
        assert result.returncode == 0 and output.exists(), f'{name} failed; inspect {log_path}'
        text = log_path.read_text(errors='replace')
        state = states[name] = read_checkpoint(output, args.vehicle, name != 'exited')
        runtime = counters(text, 'VEHICLE', 16)
        assert runtime[5] == 0 and runtime[3] == int(name != 'exited')
        assert runtime[2] == int(name == 'exited'), 'Expected one validated exit'
        if name == 'saved':
            assert runtime[1] == 1 and state['primary'] < 999, 'No accepted gunfire before save'
            assert any(abs(v) > .001 for v in state['aim']), 'Aim setup did not change view'
            if args.vehicle == 'apc':
                assert state['secondary'] < 15, 'No accepted rocket before save'
        else:
            assert 'PLAYER_CHECKPOINT_LOAD ' in text
            for key in ('primary', 'random', 'health'):
                assert state[key] == states['saved'][key], f'{key} changed across reload/exit'
            if args.vehicle == 'apc':
                assert state['secondary'] == states['saved']['secondary']
        if name == 'resumed':
            assert state['aim'] == states['saved']['aim'], 'Relative aim was not retained'
            if args.vehicle == 'jeep':
                assert state['aim_reference'] == states['saved']['aim_reference'], 'Independent gunner basis was not retained'
        if args.vehicle == 'jeep':
            seats = counters(text, 'JEEP_SEATS', 8)
            assert seats[4] == int(name != 'exited'), 'Runtime seat role differs from checkpoint'
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED', states=states)
    (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
