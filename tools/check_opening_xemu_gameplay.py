"""Audit saved text/guest-memory evidence for the real-spawn weapon handoff.

The broad renderer replay can fail separately; this never changes its result.
"""
import hashlib
import json
from pathlib import Path
import sys


root = Path(__file__).resolve().parents[1]
run = Path(sys.argv[1]) if len(sys.argv) == 2 else None
if run is None or not run.is_dir():
    raise SystemExit('Usage: python tools/check_opening_xemu_gameplay.py artifacts/xemu/replay-DATE')
report = json.loads((run / 'report.json').read_text())
guest = json.loads((run / 'guest-memory-final.json').read_text())
pc_lines = (run / 'pc-reference.txt').read_text().splitlines()

def pc_words(label):
    return [int(x) for x in next(line for line in pc_lines
                            if line.startswith(label + ' ')).split()[1:]]

source = root / 'artifacts/opening-handoff/authored-spawn.bin'
assert report['level'] == report['final_level'] == 'L1S1.rfl'
assert report['frames'] == 2380
assert report['input_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
assert report['memory'] == {'base-memory': 64*1024*1024, 'plugged-memory': 0}
assert report['samples'][-1][0] == 0x52464447  # guest diagnostic signature
for label, key in [('PLAYER_AMMO', 'player_ammo'),
                   ('PLAYER_LIFE', 'player_life'),
                   ('WEAPON_SELECTION', 'weapon_selection'),
                   ('COMBAT', 'combat')]:
    assert report[key] == pc_words(label), (label, report[key], pc_words(label))

grants = guest['symbols']['rf_scene_script_grants']['words']
assert grants == pc_words('SCRIPT_GRANTS') == [1, 1, 100, 2, 1, 100, 0, 0]
assert report['player_life'][:3] == [0, 0, 0]
assert report['player_ammo'][:3] == [2, 0, 100]
assert report['weapon_selection'][0] == 2
assert report['combat'][0] == 1
assert pc_words('TRIGGER_CONTACTS')[2] == 9869

if report['result'] == 'FAIL':
    assert report['error'] == 'AssertionError([2380, 15, 0, 2166136261, 444880])'
    assert pc_words('NPC_DRAW') == [2380, 9, 9255, 55535536, 444880]
else:
    assert report['result'] == 'PASS', report['result']

print(json.dumps(dict(gameplay='PASS', broad_replay=report['result'],
                      frames=report['frames'], grants=grants,
                      player_ammo=report['player_ammo'][:3],
                      attacks=report['combat'][0],
                      limitation='Broad NPC draw-count comparison fails; no visual correctness claim.'
                      if report['result'] == 'FAIL' else None), indent=2))
