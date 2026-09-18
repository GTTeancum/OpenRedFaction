"""Prepare/run a bounded process-local parked Driller checkpoint continuation.

Default only writes inputs and a command manifest. --run invokes the PC replay
process, never host input. This establishes state continuation, not visuals or
seated save support. --restore-health provides an explicit altered-state probe.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def vehicle(data):
    magic, version, total, size, profile, player, terrain, remote = struct.unpack_from('<4s7I', data)
    assert magic == b'RFCP' and version == 3 and total == len(data) and size == 128
    offset = 32 + player + terrain + remote
    assert offset + size == len(data)
    blob = data[offset:]
    assert blob[:4] == b'RFVC' and struct.unpack_from('<2I', blob, 4) == (1, 128)
    checksum = 2166136261
    for i, value in enumerate(blob):
        checksum = ((checksum ^ (0 if 12 <= i < 16 else value)) * 16777619) & 0xffffffff
    assert checksum == struct.unpack_from('<I', blob, 12)[0]
    result = {'flags': struct.unpack_from('<I', blob, 20)[0],
              'health': struct.unpack_from('<f', blob, 24)[0],
              'position': struct.unpack_from('<3f', blob, 32),
              'basis': struct.unpack_from('<9f', blob, 44),
              'velocity': struct.unpack_from('<3f', blob, 80),
              'angular_velocity': struct.unpack_from('<3f', blob, 92),
              'cuts': struct.unpack_from('<I', blob, 104)[0]}
    assert result['flags'] == 1, 'Fixture must remain alive and unoccupied'
    assert all(math.isfinite(v) for key in ('position', 'basis', 'velocity', 'angular_velocity') for v in result[key])
    return offset, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'artifacts/driller-checkpoint-replay')
    parser.add_argument('--exe', type=Path, default=ROOT / 'build/pc/Release/rf_pc_play.exe')
    parser.add_argument('--settle', type=int, default=180)
    parser.add_argument('--resume', type=int, default=30)
    parser.add_argument('--restore-health', type=float)
    parser.add_argument('--run', action='store_true')
    args = parser.parse_args()
    if not 60 <= args.settle <= 600 or not 1 <= args.resume <= 120:
        parser.error('Use60..600 settle and1..120 resume frames')
    if args.restore_health is not None and not 0 < args.restore_health < 900:
        parser.error('Altered-state health must be between0 and900')
    folder = args.output_dir.resolve(); folder.mkdir(parents=True, exist_ok=True)
    common = {'RF_REPLAY_LEVEL': 'ctf06.rfl', 'RF_REPLAY_ARCHIVE': 'levelsm.vpp',
              'RF_REPLAY_DEV_ROOM': '1', 'RF_REPLAY_VEHICLE': '1', 'RF_REPLAY_PLAYER_CHECKPOINT': '1'}
    jobs = []
    for name, frames in [('saved', args.settle), ('continued', args.resume), ('control', args.settle + args.resume)]:
        base = folder / name
        base.with_suffix('.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(frames * 48))
        env = dict(common, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
        if name == 'continued':
            env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(folder / ('altered.rfcp' if args.restore_health is not None else 'saved.rfcp'))
        command = [str(args.exe.resolve()), '--spawn-replay', str(ROOT / 'Installed_Game'),
                   str(base.with_suffix('.bin')), str(base.with_suffix('.ppm'))]
        jobs.append({'name': name, 'frames': frames, 'env': env, 'command': command})
    manifest = {'status': 'PREPARED_NOT_RUN', 'cwd': str(ROOT), 'jobs': jobs,
                'scope': 'Parked unoccupied vehicle; RFVC state continuation only. Inspect native frames separately.',
                'restore_health': args.restore_health}
    (folder / 'recipe.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if not args.run:
        print(json.dumps(manifest, indent=2)); return
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
    states = {}
    for job in jobs:
        name = job['name']; output = folder / (name + '.rfcp')
        output.unlink(missing_ok=True)  # only this fixture's generated checkpoint
        with (folder / (name + '.log')).open('wb') as log:
            result = subprocess.run(job['command'], cwd=ROOT, env=dict(env, **job['env']),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
        assert result.returncode == 0 and output.exists(), f'{name} failed; inspect its log'
        data = output.read_bytes(); offset, states[name] = vehicle(data)
        if name == 'saved' and args.restore_health is not None:
            altered = bytearray(data); struct.pack_into('<f', altered, offset + 24, args.restore_health)
            struct.pack_into('<I', altered, offset + 12, 0)
            checksum = 2166136261
            for value in altered[offset:]:
                checksum = ((checksum ^ value) * 16777619) & 0xffffffff
            struct.pack_into('<I', altered, offset + 12, checksum)
            vehicle(altered); (folder / 'altered.rfcp').write_bytes(altered)
    assert 'PLAYER_CHECKPOINT_LOAD ' in (folder / 'continued.log').read_text(errors='replace')
    expected = args.restore_health if args.restore_health is not None else states['saved']['health']
    assert abs(states['continued']['health'] - expected) < .001, 'Vehicle health was not restored'
    assert states['continued']['cuts'] == states['saved']['cuts']
    # One frame phase at startup is permitted; a respawn to parked baseline is
    # not hidden by broad position tolerances. Record all deltas for inspection.
    deltas = {key: max(abs(a-b) for a, b in zip(states['continued'][key], states['control'][key]))
              for key in ('position', 'basis', 'velocity', 'angular_velocity')}
    assert deltas['position'] < .05 and deltas['basis'] < .01, deltas
    assert deltas['velocity'] < .1 and deltas['angular_velocity'] < .1, deltas
    manifest.update(status='STATE_PASS_VISUALS_UNVERIFIED', states=states, continuation_deltas=deltas)
    (folder / 'report.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
