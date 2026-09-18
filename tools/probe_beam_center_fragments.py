"""Bounded ordinary beam shots seeking actual radius>1 polygon-contact rubble."""
import json
import math
import os
from pathlib import Path
import struct
import subprocess
from replay_authored_post import pitch_commands, pitch_for

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/beam-center-fragments'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
eye = json.loads((ROOT / 'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_PLAYER_CHECKPOINT='1',
           RF_REPLAY_AUTHORED_SOURCE='95', RF_REPLAY_AUTHORED_SOURCES='3')
report = []
(OUT / 'report.json').write_text('[]\n')
for index, z in enumerate((0., .5, 1.)):
    target = [-4.699, 2.25, z]
    data = bytearray(source[:8 + 350 * 48] + bytes(250 * 48))
    for offset, start, end in ((12, 0, pitch_for(eye, target)),
                               (16, -math.pi / 2, math.atan2(target[0] - eye[0], target[2] - eye[2]))):
        commands, _ = pitch_commands(start, end)
        for i, value in enumerate(commands):
            struct.pack_into('<f', data, 8 + (190 + i) * 48 + offset, value)
    base = OUT / str(index)
    base.with_suffix('.bin').write_bytes(data)
    base.with_suffix('.rfcp').unlink(missing_ok=True)
    local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(base.with_suffix('.rfcp')))
    with base.with_suffix('.log').open('wb') as log:
        result = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                                 str(ROOT / 'Installed_Game'), str(base.with_suffix('.bin')),
                                 str(base.with_suffix('.ppm'))], cwd=ROOT, env=local,
                                stdout=log, stderr=subprocess.STDOUT, timeout=180)
    row = dict(target=target, exit=result.returncode, fragments=[])
    if not result.returncode:
        saved = base.with_suffix('.rfcp').read_bytes()
        cursor = 0
        while True:
            cursor = saved.find(b'RFPB', cursor)
            if cursor < 0:
                break
            version, size, count = struct.unpack_from('<III', saved, cursor + 4)
            assert version == 2 and size == 16 + count * 328
            for i in range(count):
                at = cursor + 16 + i * 328
                row['fragments'].append(dict(radius=struct.unpack_from('<f', saved, at + 256)[0],
                    position=struct.unpack_from('<3f', saved, at + 100)))
            cursor += size
    report.append(row)
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(row), flush=True)
assert all(row['exit'] == 0 for row in report), 'Inspect failed shot logs'
assert any(piece['radius'] > 1 for row in report for piece in row['fragments']), 'No polygon-contact fragment found'
