"""Release real lifted beam support with a single angular impulse; no host input."""
import json
import argparse
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--large', action='store_true', help='Zero-inertia large beam negative control')
args = parser.parse_args()
OUT = ROOT / ('artifacts/rotating-large-rubble-support' if args.large else 'artifacts/rotating-rubble-support')
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'report.json').unlink(missing_ok=True)
(OUT / 'neutral.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + bytes(242 * 48))
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_PLAYER_CHECKPOINT='1',
           RF_REPLAY_AUTHORED_SOURCE='95' if args.large else '94', RF_REPLAY_AUTHORED_SOURCES='3' if args.large else '1',
           RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(ROOT / ('artifacts/large-rubble-standing/standing.rfcp' if args.large else 'artifacts/geomod-postedit-re/intermediate-rubble-standing/saved.rfcp')))
report = {}
checkpoint = Path(env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']).read_bytes()
bank = checkpoint.find(b'RFPB')
assert bank >= 0 and struct.unpack_from('<I', checkpoint, bank + 12)[0] == 1
inverse_tensors = struct.unpack_from('<18f', checkpoint, bank + 16 + 12 + 16)
assert all(v == 0 for v in inverse_tensors) if args.large else any(v != 0 for v in inverse_tensors)
def floats(words):
    return list(struct.unpack('<' + 'f' * len(words), struct.pack('<' + 'I' * len(words), *words)))
for name, mode in (('drop', '2'), ('spin', '3')):
    (OUT / f'{name}.rfcp').unlink(missing_ok=True)
    local = dict(env, RF_REPLAY_MOVING_SUPPORT_TEST=mode, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(OUT / f'{name}.rfcp'))
    with (OUT / f'{name}.log').open('wb') as log:
        result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                                 str(ROOT / 'Installed_Game'), str(OUT / 'neutral.bin'),
                                 str(OUT / f'{name}.ppm')], cwd=ROOT, env=local,
                                stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, (name, result.returncode)
    lines = (OUT / f'{name}.log').read_text().splitlines()
    rows = [list(map(int, line.split()[1:])) for line in lines if line.startswith('MOVING_SUPPORT ')]
    rotations = [list(map(int, line.split()[1:])) for line in lines if line.startswith('ROTATING_SUPPORT ')]
    assert len(rows) == 10
    report[name] = dict(rows=rows, rotations=rotations,
                       decoded=[dict(frame=r[0], mode=r[2], support=r[3],
                                     player=floats(r[4:7]), body=floats(r[7:10]),
                                     carry=floats(r[10:13])) for r in rows])
    print(name, json.dumps(report[name]['decoded']), flush=True)
    if rotations:print('orientation/angular', json.dumps([floats(r) for r in rotations]), flush=True)
spin = report['spin']; drop = report['drop']
assert all(r[2] == 1 and r[3] and r[13] for r in spin['rows']), 'Lost support in this trajectory'
assert all(r[4] == spin['rows'][0][4] and r[6] == spin['rows'][0][6] for r in spin['rows']), 'Uncommanded lateral player movement'
assert len(spin['rotations']) == 10
for rotation in spin['rotations']:
    matrix = floats(rotation[:9])
    assert all(math.isfinite(v) for v in matrix)
    for i in range(3):
        for j in range(3):
            assert abs(sum(matrix[i*3+k]*matrix[j*3+k] for k in range(3)) - (i == j)) < .00001, 'Invalid rotation basis'
assert spin['rotations'][-1][9:] == [0, 0, 0], 'Angular motion did not settle'
if args.large:
    assert all(r[:9] == spin['rotations'][0][:9] for r in spin['rotations']), 'Zero-inertia control rotated'
    assert spin['rows'][-1][4:13] == drop['rows'][-1][4:13], 'Zero-inertia control changed player/body endpoint'
else:
    assert max(abs(a-b) for a,b in zip(floats(spin['rotations'][6][:9]),floats(spin['rotations'][0][:9]))) > .3, 'No meaningful rotation'
    assert spin['rotations'][5][10] != 0, 'Missing angular motion during fall'
    assert abs(floats(spin['rows'][-1][5:6])[0] - floats(drop['rows'][-1][5:6])[0]) > .03, 'Player support ignored rotated shape'
report['result'] = 'PASS'
report['scope'] = 'Artificial lift and one angular impulse, then ordinary rotation/gravity/contact on real extracted support; no retail angular-carry parity claim.'
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS rotating support' if not args.large else 'PASS zero-inertia negative control')
