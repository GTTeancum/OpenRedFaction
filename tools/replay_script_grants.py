"""Authored Riot Stick inventory grant before initialization and during play."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/script-grants'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI4' + struct.pack('<I', 40) + bytes(90 * 40))
cases = []
for name, setup, expected in [('once', '9870', [1, 1, 1, 2, 1, 1, 0, 0]),
                               ('twice', '9870,9870', [2, 1, 2, 2, 1, 1, 1, 0])]:
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_SETUP_UID=setup)
    run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                          '--spawn-replay', str(root / 'Installed_Game'),
                          str(source), str(folder / (name + '.ppm'))],
                         env=env, capture_output=True, text=True)
    (folder / (name + '.log')).write_text(run.stdout + run.stderr)
    run.check_returncode()
    grant = list(map(int, next(s for s in run.stdout.splitlines()
                              if s.startswith('SCRIPT_GRANTS ')).split()[1:]))
    assert grant == expected, grant
    assert 'Completed 90 frames' in run.stdout
    cases.append(dict(case=name, grants=grant))
report = dict(result='PASS', cases=cases,
              scope='Explicit Give_Item_To_Player9870 at0 and optional repeat at60. '
                    'Riot Stick ownership/ammo only; selection, melee and presentation unfinished.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
