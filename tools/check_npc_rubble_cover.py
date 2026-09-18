"""Rocket extraction, staged armed NPC, held real cover and removal control."""
import json
import os
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'artifacts/npc-rubble-cover'
out.mkdir(exist_ok=True)
source = (ROOT / 'artifacts/beam-center-fragments/1.bin').read_bytes()
assert source[:8] == b'RFI6' + struct.pack('<I', 48) and len(source) == 8 + 600 * 48
inputs = out / 'live.bin'
inputs.write_bytes(source + bytes(362 * 48))
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_DEV_ROOM='1', RF_REPLAY_DEV_NPC='2', RF_REPLAY_LEVEL='ctf06.rfl',
           RF_REPLAY_ARCHIVE='levelsm.vpp', RF_REPLAY_AUTHORED_SOURCE='95',
           RF_REPLAY_AUTHORED_SOURCES='3')
run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                      str(ROOT / 'Installed_Game'), str(inputs), str(out / 'live.ppm')],
                     env=env, capture_output=True, text=True)
(out / 'live.log').write_text(run.stdout + run.stderr)
run.check_returncode()
rows = [list(map(int, line.split()[1:])) for line in run.stdout.splitlines()
        if line.startswith('DEV_NPC_COVER ')]
assert len(rows) == 6
for i, row in enumerate(rows):
    health = struct.unpack('<f', struct.pack('<I', row[4]))[0]
    assert row[0] == 660 + i * 60 and row[1] == i + 1
    if i < 3:
        assert row[2:4] == [0, i + 1] and health == 100 and row[5] == 1
    else:
        assert row[2:4] == [i - 2, 3] and health < 100 and row[5] == 0
report = dict(result='PASS', rows=rows,
              scope='Three ordinary NPC shots intercepted by a held rocket-extracted fragment; '
                    'three player hits after explicit retirement. NPC placement and held cover are test stimuli. '
                    'No NPC-created destruction, natural resting-cover placement or audible-quality claim.')
(out / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
