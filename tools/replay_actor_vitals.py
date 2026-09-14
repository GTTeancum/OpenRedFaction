"""Shoot a living guard and retain vitals through two controlled section exits."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--mission-event', action='store_true', help='Dispatch authored Make_Invulnerable9668 before the round trip')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/actor-vitals'
folder.mkdir(exist_ok=True)
source = folder / 'inputs.bin'
source.write_bytes(b'RFI4' + struct.pack('<I', 40) + b''.join(
    struct.pack('<5f5I', 0, 0, 0, 0, 0, 0, 0, 0, int(i == 30), 0)
    for i in range(240)))
env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ACTOR_UID='8456',
           RF_REPLAY_EXIT_UID='9019', RF_REPLAY_RETURN_EXIT_UID='9346')
if args.mission_event:
    env['RF_REPLAY_SETUP_UID'] = '9668'
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
                      '--spawn-replay', str(root / 'Installed_Game'),
                      str(source), str(folder / 'final.ppm')],
                     env=env, capture_output=True, text=True)
(folder / 'pc.log').write_text(run.stdout + run.stderr)
run.check_returncode()
lines = run.stdout.splitlines()
transitions = [s for s in lines if s.startswith('LEVEL_TRANSITION ')]
assert transitions == ['LEVEL_TRANSITION L1S1.rfl L1S2.rfl 9019 61',
                       'LEVEL_TRANSITION L1S2.rfl L1S1.rfl 9346 181'], transitions
vitals = next(s for s in lines if s.startswith('ACTOR_VITALS l1s1.rfl 8456 '))
health, armor = struct.unpack('<2f', struct.pack('<2I', *map(int, vitals.split()[3:])))
assert abs(health - 80.8) < .0001 and abs(armor - 79.2) < .0001, vitals
assert 'DEFEATED_ACTOR l1s1.rfl 8456' not in lines
assert 'Completed 240 frames' in run.stdout
mission = next(s for s in lines if s.startswith('ACTOR_MISSION_STATE l1s1.rfl 9643 '))
assert mission == f'ACTOR_MISSION_STATE l1s1.rfl 9643 1 {4 if args.mission_event else 0}', mission
report = dict(result='PASS', transitions=transitions, health=health, armor=armor, mission=mission,
              scope='Staged aim, real shot and explicit exits; verifies retained living vitals. '
                    'Restoration into live owners is covered by scene tests. No natural route claim.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(report)
