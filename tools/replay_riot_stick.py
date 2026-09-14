"""Grant/select the Riot Stick, approach or withdraw, and attempt a primary strike."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/riot-stick'
folder.mkdir(exist_ok=True)
cases = []
for name, direction, end in [('far', -1, 75), ('near', 1, 25)]:
    source = folder / (name + '.bin')
    source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(
        struct.pack('<5f6I', 0, 0, float(direction if 10 <= i < end else 0),
                    0, 0, 0, 0, 0, int(i == 120), 0, int(i == 5))
        for i in range(150)))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ACTOR_UID='8456',
               RF_REPLAY_SETUP_UID='9870')
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / (name + '.ppm'))],
                         env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    def words(label):
        return list(map(int, next(s for s in run.stdout.splitlines()
                                 if s.startswith(label + ' ')).split()[1:]))
    combat, ammo, selection, model = [words(s) for s in
                                     ['COMBAT', 'PLAYER_AMMO', 'WEAPON_SELECTION', 'PLAYER_WEAPON']]
    # The opening strip leaves only the granted baton; selection is automatic.
    assert selection[:2] == [2, 0] and ammo[:3] == [2, 0, 100], (selection, ammo)
    assert combat[0] == 1 and combat[1] == int(name == 'near') and combat[7] == 0, combat
    assert model[2] > 0 and model[6] == 0 and ammo[7] == 0, (model, ammo)
    assert 'Completed 150 frames' in run.stdout
    cases.append(dict(case=name, combat=combat, ammo=ammo, model=model))
report = dict(result='PASS', cases=cases,
              scope='Authored grant plus process-local actor staging; actual movement, selection and primary strike. '
                    'Near hit/far miss with unchanged charge. Alternate fire has separate coverage; precise impact timing remains open.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
