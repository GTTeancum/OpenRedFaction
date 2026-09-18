"""Jump onto an actually extracted large beam fragment; no inflated test body."""
import json
import argparse
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
from replay_authored_post import pitch_commands, pitch_for

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--no-save', action='store_true', help='Inspect contact without checkpoint export')
args = parser.parse_args()
OUT = ROOT / 'artifacts/large-rubble-standing'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'report.json').unlink(missing_ok=True)
eye = json.loads((ROOT / 'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
target = [-4.699, 2.25, .5]
data = bytearray(b'RFI6' + struct.pack('<I', 48) + bytes(351 * 48))
for offset, start in ((12, pitch_for(eye, target)),
                      (16, math.atan2(target[0] - eye[0], target[2] - eye[2]))):
    commands, _ = pitch_commands(start, 0 if offset == 12 else -math.pi / 2)
    for i, value in enumerate(commands):
        struct.pack_into('<f', data, 8 + (10 + i) * 48 + offset, value)
for frame in range(40, 60):
    struct.pack_into('<f', data, 8 + frame * 48, -.5)
for frame in range(80, 158):
    struct.pack_into('<f', data, 8 + frame * 48 + 8, .8)
struct.pack_into('<I', data, 8 + 130 * 48 + 24, 1)
(OUT / 'standing.bin').write_bytes(data)
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_PLAYER_CHECKPOINT='1',
           RF_REPLAY_AUTHORED_SOURCE='95', RF_REPLAY_AUTHORED_SOURCES='3',
           RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(ROOT / 'artifacts/beam-center-fragments/1.rfcp'),
           RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(OUT / 'standing.rfcp'))
if args.no_save:env.pop('RF_REPLAY_GEOMOD_CHECKPOINT_OUT')
else:(OUT / 'standing.rfcp').unlink(missing_ok=True)
with (OUT / 'standing.log').open('wb') as log:
    result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                             str(ROOT / 'Installed_Game'), str(OUT / 'standing.bin'),
                             str(OUT / 'standing.ppm')], cwd=ROOT, env=env,
                            stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert result.returncode == 0, result.returncode
lines = (OUT / 'standing.log').read_text().splitlines()
def values(label, kind=int):
    return list(map(kind, next(line for line in lines if line.startswith(label + ' ')).split()[1:]))
report = dict(position=values('CAMPAIGN_FINAL_POSITION', float), contacts=values('DETACHED_PLAYER'),
              pieces=values('DETACHED_PIECES'))
print(json.dumps(report, indent=2))
if args.no_save:
    report['scope'] = 'Contact inspection only; no save acceptance.'
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    sys.exit(0)
assert -5.4 < report['position'][0] < -4.6 and report['position'][1] > 0
assert report['contacts'][1] > 0 and report['contacts'][2] != 0xffffffff and report['contacts'][6] == 0
saved = (OUT / 'standing.rfcp').read_bytes()
bank = saved.find(b'RFPB')
assert bank >= 0 and struct.unpack_from('<I', saved, bank + 12)[0] == 1
radius = struct.unpack_from('<f', saved, bank + 16 + 256)[0]
assert radius > 1, radius
report['radius'] = radius
neutral = b'RFI6' + struct.pack('<I', 48) + bytes(121 * 48)
retreat = bytearray(b'RFI6' + struct.pack('<I', 48) + bytes(201 * 48))
for frame in range(20, 65):struct.pack_into('<f', retreat, 8 + frame * 48 + 8, -.8)
for name, recording in (('continued', neutral), ('control', data + bytes(120 * 48)),
                        ('retreat', retreat), ('retreat-control', data + retreat[8 + 48:])):
    base = OUT / name
    base.with_suffix('.bin').write_bytes(recording)
    base.with_suffix('.rfcp').unlink(missing_ok=True)
    local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
    if name in ('continued', 'retreat'):local['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(OUT / 'standing.rfcp')
    with base.with_suffix('.log').open('wb') as log:
        result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                                 str(ROOT / 'Installed_Game'), str(base.with_suffix('.bin')),
                                 str(base.with_suffix('.ppm'))], cwd=ROOT, env=local,
                                stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, (name, result.returncode)
    if name.startswith('retreat'):
        rows = base.with_suffix('.log').read_text().splitlines()
        position = list(map(float, next(r for r in rows if r.startswith('CAMPAIGN_FINAL_POSITION ')).split()[1:]))
        assert position[0] > -2 and position[1] < 0, (name, position)
        report[name] = dict(position=position)
assert (OUT / 'continued.rfcp').read_bytes() == (OUT / 'control.rfcp').read_bytes(), 'Standing continuation differs'
assert (OUT / 'retreat.rfcp').read_bytes() == (OUT / 'retreat-control.rfcp').read_bytes(), 'Walk-away continuation differs'
# Counterfactual: same elevated player pose, but its only fragment is retired.
# Loading must reject the unsupported placement rather than accept a floating save.
missing = bytearray(saved)
bank_bytes = struct.unpack_from('<I', missing, bank + 8)[0]
struct.pack_into('<fI', missing, bank + bank_bytes - 8, -1, 0x200002)
(OUT / 'missing-support.rfcp').write_bytes(missing)
local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(OUT / 'missing-support.rfcp'))
local.pop('RF_REPLAY_GEOMOD_CHECKPOINT_OUT')
with (OUT / 'missing-support.log').open('wb') as log:
    result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                             str(ROOT / 'Installed_Game'), str(OUT / 'continued.bin'),
                             str(OUT / 'missing-support.ppm')], cwd=ROOT, env=local,
                            stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert result.returncode != 0 and 'GEOMOD_CHECKPOINT_ERROR load' in (OUT / 'missing-support.log').read_text(), 'Missing large support accepted'
# Same retired bank, but place the player at the verified walk-away endpoint.
# This distinguishes support rejection from rejection of the retirement encoding.
floor = bytearray(missing)
floor[64:76] = (OUT / 'retreat.rfcp').read_bytes()[64:76]
(OUT / 'retired-floor.rfcp').write_bytes(floor)
local['RF_REPLAY_GEOMOD_CHECKPOINT_IN'] = str(OUT / 'retired-floor.rfcp')
with (OUT / 'retired-floor.log').open('wb') as log:
    result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                             str(ROOT / 'Installed_Game'), str(OUT / 'continued.bin'),
                             str(OUT / 'retired-floor.ppm')], cwd=ROOT, env=local,
                            stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert result.returncode == 0, 'Retired bank itself was invalid'
report['missing_support_rejected'] = True
report['same_retired_bank_floor_accepted'] = True
# A process-local look down at the actual support gives useful visual evidence
# without changing the accepted standing/checkpoint fixture.
view = bytearray(b'RFI6' + struct.pack('<I', 48) + bytes(181 * 48))
for offset, start, end in ((12, 0, -.65), (16, -math.pi / 2, .16)):
    commands, _ = pitch_commands(start, end, 120)
    for i, value in enumerate(commands):struct.pack_into('<f', view, 8 + i * 48 + offset, value)
(OUT / 'view.bin').write_bytes(view)
local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(OUT / 'standing.rfcp'))
local.pop('RF_REPLAY_GEOMOD_CHECKPOINT_OUT')
with (OUT / 'view.log').open('wb') as log:
    result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                             str(ROOT / 'Installed_Game'), str(OUT / 'view.bin'), str(OUT / 'view.ppm')],
                            cwd=ROOT, env=local, stdout=log, stderr=subprocess.STDOUT, timeout=180)
assert result.returncode == 0, ('view', result.returncode)
report.update(result='PASS', exact_continuation=True, exact_walk_away=True, scope='Ordinary jump onto real radius>1 fragment, polygon contacts, standing save and exact PC standing/walk-away continuation; native acceptance separate.')
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS large fragment standing and continuation')
