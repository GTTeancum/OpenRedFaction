"""Ordinary staged NPC encounter: authored handgun damage reaches player death."""
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'artifacts/npc-authored-damage'
out.mkdir(exist_ok=True)
source = out / 'idle.bin'
source.write_bytes(b'RFI4' + struct.pack('<I', 40) + bytes(240 * 40))
env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_ACTOR_UID='8456', RF_REPLAY_LEVEL='L1S1.rfl',
           RF_REPLAY_ARCHIVE='levels1.vpp')
exe = root / 'build/pc/Release/rf_pc_play.exe'
run = subprocess.run([str(exe), '--spawn-replay', str(root / 'Installed_Game'),
                      str(source), str(out / 'idle.ppm')], env=env,
                     capture_output=True, text=True)
(out / 'idle.log').write_text(run.stdout + run.stderr)
run.check_returncode()

def row(label):
    return list(map(int, next(line for line in run.stdout.splitlines()
                             if line.startswith(label + ' ')).split()[1:]))

enemy, kinds, fire = row('ENEMY_COMBAT'), row('ENEMY_DAMAGE_KINDS'), row('ENEMY_FIRE')
health = struct.unpack('<f', struct.pack('<I', enemy[5]))[0]
assert enemy[:5] == [240, 2, 5, 5, 0] and enemy[6:] == [1, 0] and health <= 0
assert kinds == [0, 5, 0, 0, 0, 0, 0, 0, 0, 0], 'Expected supported bullet hits, no fallback'
assert fire[0:4] == [5, 5, 0, 5] and fire[5] == 0, 'Missing firing clip/audio dispatch'
assert row('WEAPON_AUDIO')[7] == 0
assert row('LIQUID_DAMAGE')[1] == 0, 'Liquid damage contaminated the encounter'
assert row('COMBAT')[0] == 0, 'Player fired in neutral-input fixture'
report = dict(result='PASS', enemy=enemy, damage_kinds=kinds, fire=fire, health=health,
              input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              scope='Staged actor8456, neutral240 ticks, ordinary NPC targeting/fire and player death. '
                    'Handgun amount40 is independently supported by original getter/default probes. '
                    'Not live NPC rubble-impact validation, audible-quality acceptance or campaign progression.')
(out / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
