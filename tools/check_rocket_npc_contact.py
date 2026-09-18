"""Real player rocket strikes the armed guard after the held-cover control."""
import json
import math
import os
from pathlib import Path
import struct
import subprocess
from replay_authored_post import pitch_for, pitch_commands

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'artifacts/rocket-npc-contact'
out.mkdir(exist_ok=True)
data = bytearray((ROOT / 'artifacts/npc-rubble-cover/live.bin').read_bytes())
assert len(data) == 8 + 962 * 48 and data[:8] == b'RFI6' + struct.pack('<I', 48)
eye = json.loads((ROOT / 'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
old, target = [-4.699, 2.25, .5], [-1.5, .1, 2.5]
for offset, start, end in (
    (12, pitch_for(eye, old), pitch_for(eye, target)),
    (16, math.atan2(old[0]-eye[0], old[2]-eye[2]), math.atan2(target[0]-eye[0], target[2]-eye[2]))):
    commands, _ = pitch_commands(start, end)
    for i, value in enumerate(commands):
        struct.pack_into('<f', data, 8 + (815+i)*48 + offset, value)
struct.pack_into('<I', data, 8 + 850*48 + 32, 1)
source = out / 'live.bin'
source.write_bytes(data)
env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
env.update(RF_REPLAY_DEV_ROOM='1', RF_REPLAY_DEV_NPC='2', RF_REPLAY_LEVEL='ctf06.rfl',
           RF_REPLAY_ARCHIVE='levelsm.vpp', RF_REPLAY_AUTHORED_SOURCE='95', RF_REPLAY_AUTHORED_SOURCES='3')
run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
                      str(ROOT / 'Installed_Game'), str(source), str(out / 'final.ppm')],
                     env=env, capture_output=True, text=True)
(out / 'run.log').write_text(run.stdout+run.stderr)
run.check_returncode()
def row(label):
    return list(map(int, next(line for line in run.stdout.splitlines() if line.startswith(label+' ')).split()[1:]))
contacts, rockets, death = row('ROCKET_CONTACTS'), row('ROCKETS'), row('COMBAT_DEATH')
assert contacts[1:4] == [1, 0, 1] and contacts[5:7] == [0x70000001, 0]
assert struct.unpack('<f', struct.pack('<I', contacts[4]))[0] == 400
assert rockets[:5] == [2, 2, 0, 0, 1], 'Actor hit must not produce a terrain cut behind it'
assert death[0] == 1 and row('ENEMY_COMBAT')[2:4] == [4, 1]
assert row('ROCKET_BLAST')[0] == 2 and row('ROCKET_BLAST')[3] == 1
report = dict(result='PASS', contacts=contacts, rockets=rockets, death=death,
              scope='One ordinary beam-cut rocket followed by one direct actor hit/death after controlled cover removal. '
                    'Body-sphere contacts; direct location multiplier1 is provisional. Mover behavior is covered separately by unit queries.')
(out / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
