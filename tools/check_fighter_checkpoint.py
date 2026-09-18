"""Prepare seated Fighter save/reload/exit replays; --run explicitly opts in.

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


def inputs(phase):
    frames = 360 if phase == 'saved' else 60
    rows = []
    for frame in range(frames):
        saved = phase == 'saved'
        rows.append(struct.pack('<5f7I', 0, 0, float(saved and 40 <= frame < 80), 0, 0,
                                0, int(saved and 90 <= frame < 105),
                                int(saved and frame == 12 or phase == 'exited' and frame == 20),
                                int(saved and 120 <= frame < 150), 0, 0, int(saved and frame == 160)))
    return frames, b'RFI6' + struct.pack('<I', 48) + b''.join(rows)


def read_checkpoint(path, occupied):
    data = path.read_bytes()
    assert len(data) >= 32, 'Truncated RFCP'
    magic, version, total, size, _, player_size, terrain_size, remote_size = struct.unpack_from('<4s7I', data)
    assert (magic, version, total, size, player_size) == (b'RFCP', 3, len(data), 160, 544)
    offset = 32 + player_size + terrain_size + remote_size
    assert offset + size == len(data)
    blob = data[offset:]
    assert blob[:4] == b'RFVC' and struct.unpack_from('<2I', blob, 4) == (5, 160)
    assert struct.unpack_from('<I', blob, 16)[0] == 5
    checksum = 2166136261
    for i, value in enumerate(blob):
        checksum = ((checksum ^ (0 if 12 <= i < 16 else value)) * 16777619) & 0xffffffff
    assert struct.unpack_from('<I', blob, 12)[0] == checksum
    assert not any(blob[128:160]), 'Reserved fighter bytes must remain zero'
    assert data[32:36] == b'RFPL'
    pl_version, pl_bytes, mode = struct.unpack_from('<3I', data, 36)
    assert pl_bytes == 544
    assert (pl_version == 3 and mode == 1) if occupied else (pl_version in (1, 2) and mode == 0)
    assert struct.unpack_from('<I', blob, 20)[0] == (3 if occupied else 1)
    state = dict(health=struct.unpack_from('<f', blob, 24)[0],
                 primary=struct.unpack_from('<I', blob, 104)[0],
                 rockets=struct.unpack_from('<I', blob, 108)[0],
                 cooldowns=struct.unpack_from('<2f', blob, 112),
                 shots=struct.unpack_from('<2I', blob, 120),
                 position=struct.unpack_from('<3f', blob, 32),
                 orientation=struct.unpack_from('<9f', blob, 44),
                 velocity=struct.unpack_from('<3f', blob, 80),
                 angular_velocity=struct.unpack_from('<3f', blob, 92), occupied=occupied)
    assert 0 < state['health'] <= 900 and 0 <= state['primary'] <= 900 and 0 <= state['rockets'] <= 20
    assert all(math.isfinite(v) for key in ('position', 'orientation', 'velocity', 'angular_velocity', 'cooldowns') for v in state[key])
    assert -.05 <= state['cooldowns'][0] <= .05 and -.05 <= state['cooldowns'][1] <= 3
    state['weapon_bytes'] = blob[104:128].hex()
    return state


def counters(text, name, count):
    rows = [list(map(int, line.split()[1:])) for line in text.splitlines() if line.startswith(name + ' ')]
    assert rows and len(rows[-1]) == count, f'Missing {name} diagnostics'
    return rows[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build/pc/Release/rf_pc_play.exe')
    parser.add_argument('--run', action='store_true')
    args = parser.parse_args()
    folder = (args.output_dir or ROOT / 'artifacts/fighter-checkpoint').resolve()
    folder.mkdir(parents=True, exist_ok=True)
    common = {'RF_REPLAY_LEVEL': 'ctf06.rfl', 'RF_REPLAY_ARCHIVE': 'levelsm.vpp',
              'RF_REPLAY_DEV_ROOM': '1', 'RF_REPLAY_VEHICLE': 'fighter', 'RF_REPLAY_PLAYER_CHECKPOINT': '1'}
    jobs = []
    for name, previous in [('saved', None), ('resumed', 'saved'), ('exited', 'resumed')]:
        base = folder / name
        frames, data = inputs(name)
        base.with_suffix('.bin').write_bytes(data)
        env = dict(common, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
        if previous:
            env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(folder / (previous + '.rfcp'))
        jobs.append({'name': name, 'frames': frames, 'env': env,
                     'command': [str(args.exe.resolve()), '--spawn-replay', str(ROOT / 'Installed_Game'),
                                 str(base.with_suffix('.bin')), str(base.with_suffix('.ppm'))]})
    report = {'status': 'PREPARED_NOT_RUN', 'vehicle': 'fighter', 'cwd': str(ROOT), 'jobs': jobs,
              'scope': 'Seated save/reload/exit state checks; visual/audio and Xbox acceptance remain separate.',
              'schedule': 'Save360: board12,forward40..79,rise90..104,primary120..149,rocket160,neutral through359; resume60 neutral; exit60 Use20.',
              'prerequisite': 'Live RFCP profile5 wiring and frontend enablement; failures are not skipped.'}
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
        state = states[name] = read_checkpoint(output, name != 'exited')
        runtime = counters(text, 'VEHICLE', 16)
        weapon = counters(text, 'FIGHTER_WEAPON', 8)
        assert runtime[5] == 0 and runtime[3] == int(name != 'exited')
        assert runtime[2] == int(name == 'exited'), 'Expected one validated exit'
        assert weapon[6:] == [state['primary'], state['rockets']]
        if name == 'saved':
            assert runtime[1] == 1 and 0 < state['primary'] < 900 and state['rockets'] == 19
            assert state['position'][1] > 7 and math.hypot(state['position'][0]-30, state['position'][2]+160) > .1
            assert weapon[5] == 0, 'Cannot save active projectiles'
        else:
            assert 'PLAYER_CHECKPOINT_LOAD ' in text
            for key in ('primary', 'rockets', 'shots', 'health', 'orientation'):
                assert state[key] == states['saved'][key], f'{key} changed across reload/exit'
            assert weapon[1] == 0 and weapon[2] == 0, 'Unexpected shot during neutral restore/exit'
            # Free hover retains velocity; decay continues after load, so a
            # whole-record byte equality would incorrectly forbid simulation.
            distance = math.sqrt(sum((a-b)**2 for a,b in zip(state['position'], states['saved']['position'])))
            assert distance < .05, 'Settled saved pose did not survive restore'
            assert sum(v*v for v in state['velocity']) <= sum(v*v for v in states['saved']['velocity']) + 1e-6
            # Cooldowns may decay/canonicalize; exact ammo + lifetime shot bytes
            # must survive even when these two timer words change.
            saved_bytes = bytes.fromhex(states['saved']['weapon_bytes'])
            current_bytes = bytes.fromhex(state['weapon_bytes'])
            assert current_bytes[:8] == saved_bytes[:8] and current_bytes[16:] == saved_bytes[16:]
    report.update(status='STATE_PASS_VISUALS_UNVERIFIED', states=states)
    (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
