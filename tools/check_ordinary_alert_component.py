"""Text-only live PC check for the bounded ordinary NPC alert component."""

import os
from pathlib import Path
import struct
import subprocess


root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/ordinary-alert'
folder.mkdir(parents=True, exist_ok=True)
route = folder / 'neutral.bin'
route.write_bytes(b'RFI6' + struct.pack('<I', 48) +
                  struct.pack('<5f7I', *([0] * 12)) * 60)

env = {key: value for key, value in os.environ.items()
       if not key.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='ctf06.rfl', RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1', RF_REPLAY_DEV_NPC='3',
           RF_REPLAY_WORLD_CHECKPOINT_PROBE='1', RF_REPLAY_TRACE='1',
           RF_REPLAY_TRACE_FROM='0')
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-telemetry-replay', str(root / 'Installed_Game'),
                      str(route)], cwd=root, env=env, capture_output=True,
                     text=True)
(folder / 'run.log').write_text(run.stdout + run.stderr)
run.check_returncode()

row = next(line.split() for line in run.stdout.splitlines()
           if line.startswith('NPC_COMBAT_ROW 1879048193 '))
component = next(line.split() for line in run.stdout.splitlines()
                 if line.startswith('WORLD_CHECKPOINT_PROBE npc '))
assert row[5] == '1', row  # ordinary combat alert on the live guard
assert component == ['WORLD_CHECKPOINT_PROBE', 'npc', 'status0', 'bytes736'], component
assert 'WORLD_CHECKPOINT_PROBE_ONLY no_save_written' in run.stdout
print('PASS live ordinary alert NPC component: 736 bytes, no save or image')
