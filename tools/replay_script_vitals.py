"""Authored delayed Heal/Armor during a controlled opening-level encounter."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/script-vitals'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI4' + struct.pack('<I', 40) + bytes(240 * 40))
cases = []
for name, setup in [('control', None), ('healed', '9959,9960')]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ACTOR_UID='8456')
    if setup:
        env['RF_REPLAY_SETUP_UID'] = setup
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / (name + '.ppm'))],
                         env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines()
                                 if s.startswith(label + ' ')).split()[1:]))
    enemy = words('ENEMY_COMBAT')
    health, armor = struct.unpack('<2f', struct.pack('<2I', *words('PICKUP_VITALS')[:2]))
    assert enemy[2:4] == [8, 8] and enemy[7] == 0, enemy
    assert 'Completed 240 frames' in run.stdout
    cases.append(dict(case=name, health=health, armor=armor, enemy=enemy))
assert abs(cases[0]['health'] - 61.6) < .0001, cases
assert abs(cases[1]['health'] - 71.2) < .0001, cases
assert cases[0]['armor'] == cases[1]['armor'], cases
report = dict(result='PASS', cases=cases,
              scope='Staged hostile encounter, authored Armor9959 at0 and Heal9960 at60; '
                    'both retain their half-second delay. Same eight hits, higher final health. '
                    'NPC targets, signed reductions and caps also covered by scene tests. No natural trigger claim.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
