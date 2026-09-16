"""Fresh-process DEV destruction checkpoint checks; no desktop input.

Geometry, admission/RNG and retained atlas state must survive a scene rebuild.
Player/weapons are not in this checkpoint: compare only the upper world view.
"""
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
from PIL import Image
from dev_destruction_check import ROOT, recording

FOLDER = ROOT / 'artifacts/geomod-checkpoint'


def replay(shots=True):
    data = bytearray(recording('double'))
    data.extend(bytes((900 - 400) * 48))
    if not shots:
        for frame in range(900):
            struct.pack_into('<I', data, 8 + frame * 48 + 32, 0)
    return bytes(data)


def run(name, payload, incoming=None, shallow=None, expect_success=True):
    inputs = FOLDER / (name + '.bin')
    inputs.write_bytes(payload)
    output = FOLDER / (name + '.rfds')
    # A failed decode must not be mistaken for a stale successful output.
    if output.exists():
        output.unlink()
    env = {k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(output),
               RF_REPLAY_TERRAIN_BASE_AUDIT=str(FOLDER / (name + '.csv')),
               RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(FOLDER / (name + '.mesh')),
               RF_REPLAY_DEPTH_OUT=str(FOLDER / (name + '.depth')))
    if incoming:
        env['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(incoming)
    if shallow:
        env['RF_REPLAY_SHALLOW_FIXTURE'] = shallow
    result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'),
        '--dev-room-replay', str(ROOT / 'Installed_Game'), str(inputs),
        str(FOLDER / (name + '.ppm'))], cwd=ROOT, env=env, capture_output=True, text=True)
    log = result.stdout + result.stderr
    (FOLDER / (name + '.log')).write_text(log)
    if not expect_success:
        assert result.returncode != 0, 'Malformed checkpoint accepted'
        assert not output.exists(), 'Failed load published a checkpoint'
        return None
    result.check_returncode()
    data = output.read_bytes()
    assert data[:4] == b'RFDS' and struct.unpack_from('<I', data, 4)[0] == 1
    assert struct.unpack_from('<I', data, 8)[0] == len(data) <= 110524
    assert len(data) >= 288
    with (FOLDER / (name + '.csv')).open() as source:
        atlas = list(csv.DictReader(source))
    # Each endpoint gets its own image, to be visually inspected by the primary.
    Image.open(FOLDER / (name + '.ppm')).save(FOLDER / (name + '.png'))
    return dict(blob=data, atlas=atlas,
                mesh=(FOLDER / (name + '.mesh')).read_bytes(),
                camera=next(line for line in log.splitlines() if line.startswith('DEPTH_CAMERA ')),
                world=Image.open(FOLDER / (name + '.ppm')).crop((0, 0, 640, 320)).tobytes())



def next_blast_case():
    uninterrupted = bytearray(replay(True))
    resumed = bytearray(replay(False))
    for data in (uninterrupted, resumed):
        struct.pack_into('<I', data, 8 + 620*48 + 32, 1)
    base = run('next-blast-control', bytes(uninterrupted))
    restored = run('next-blast-restored', bytes(resumed), FOLDER / 'repeated.rfds')
    assert struct.unpack_from('<I', base['blob'], 300)[0] == 3, 'Third blast did not publish a new cut'
    assert struct.unpack_from('<I', base['blob'], 240)[0] == 3, 'Third admission missing'
    for field in ('blob', 'mesh', 'atlas', 'camera', 'world'):
        assert base[field] == restored[field], 'Continued blast differs: ' + field
    print('PASS: next blast after restore matches uninterrupted geometry, RNG, atlas and upper world', flush=True)
    return dict(case='next blast', bytes=len(base['blob']), maps=len(base['atlas']), exact=True)


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    results = []
    for name, shots, shallow in [('empty', False, None), ('repeated', True, None),
                                  ('shallow', True, '2'), ('reset', True, None)]:
        payload = replay(shots)
        if name == 'reset':
            payload = bytearray(payload)
            for offset in (20,28,44):
                struct.pack_into('<I', payload, 8 + 500*48 + offset, 1)
            payload = bytes(payload)
        base = run(name, payload, shallow=shallow)
        restored = run(name + '-restored', replay(False), FOLDER / (name + '.rfds'), shallow)
        assert base['blob'] == restored['blob'], name + ': saved state changed after fresh load'
        assert base['mesh'] == restored['mesh'], name + ': physical geometry changed'
        assert base['atlas'] == restored['atlas'], name + ': retained map contents/bindings changed'
        assert base['camera'] == restored['camera'], name + ': comparison camera changed'
        assert base['world'] == restored['world'], name + ': upper world pixels changed'
        results.append(dict(case=name, bytes=len(base['blob']), maps=len(base['atlas']), exact=True))
        print('PASS:', name, 'fresh scene geometry, checkpoint, atlas and upper world', flush=True)
    after_empty = run('empty-next-blasts', replay(True), FOLDER / 'empty.rfds')
    assert after_empty['blob'] == (FOLDER / 'repeated.rfds').read_bytes(), 'Empty restore changed first-blast RNG or bindings'
    bad = bytearray((FOLDER / 'repeated.rfds').read_bytes())
    bad[4] = 2
    bad_path = FOLDER / 'bad-version.rfds'
    bad_path.write_bytes(bad)
    run('bad-version', replay(False), bad_path, expect_success=False)
    run('wrong-regions', replay(False), FOLDER / 'shallow.rfds', expect_success=False)
    for name, offset, value in [('bad-map-count', 248, 1025), ('bad-identity', 80, 0)]:
        bad = bytearray((FOLDER / 'repeated.rfds').read_bytes())
        if name == 'bad-identity':
            bad[offset] ^= 1
        else:
            struct.pack_into('<I', bad, offset, value)
        path = FOLDER / (name + '.rfds')
        path.write_bytes(bad)
        run(name, replay(False), path, expect_success=False)
    bad = bytearray((FOLDER / 'repeated.rfds').read_bytes()[:-1])
    struct.pack_into('<I', bad, 8, len(bad))
    path = FOLDER / 'truncated.rfds'
    path.write_bytes(bad)
    run('truncated', replay(False), path, expect_success=False)
    results.append(next_blast_case())
    report = dict(cases=results, rejected=['version', 'region identity', 'source identity', 'map count', 'truncated body'],
        scope='Fresh PC scene destruction only. Exact upper320 viewport rows exclude unsaved weapon/HUD state. No Xbox runtime or complete game-save claim.')
    (FOLDER / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
