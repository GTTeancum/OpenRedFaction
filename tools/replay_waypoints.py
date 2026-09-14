"""Walk across the opening waypoint trigger; no direct event activation."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/waypoints'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(
    struct.pack('<5f6I', 0, 0, float(30 <= i < 210), 0, 0, 0, 0, 0, 0, 0, 0)
    for i in range(2400)))
env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_EXIT_START='9646')
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
    '--spawn-replay', str(root / 'Installed_Game'), str(source), str(folder / 'end.ppm')],
    env=env, capture_output=True, text=True)
(folder / 'run.log').write_text(run.stdout + run.stderr)
run.check_returncode()
def words(label):
    return list(map(int, next(s for s in run.stdout.splitlines()
        if s.startswith(label + ' ')).split()[1:]))
move, actor, triggers = words('SCRIPT_MOVE'), words('SCRIPT_ACTOR'), words('NPC_TRIGGERS')
position = struct.unpack('<3f', struct.pack('<3I', *actor[1:4]))
assert move[0] == 1 and move[1] > 0 and move[2:5] == [2, 0, 0] and move[5:8] == [8322, 9646, 0], move
assert actor[0] == 8322, actor
assert (position[0] + 75.031525)**2 + (position[2] - 27.972010)**2 < .25**2, position
assert triggers[1:4] == [1, 8322, 9672], triggers
assert 'Completed 2400 frames' in run.stdout
report = dict(result='PASS', movement=move, miner_position=position, npc_triggers=triggers,
    scope='Staged before player trigger9029; ordinary walking activates Follow_Waypoints9646. '
          'Miner8322 reaches both authored destinations and contacts NPC trigger9672. '
          'Does not cross player trigger9028 or prove the full confrontation/item handoff.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
