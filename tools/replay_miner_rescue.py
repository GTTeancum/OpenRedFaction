"""Probe the authored L2S2a rescue contact; records evidence, not a traversal pass."""
import json
import os
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/miner-rescue-replay'
folder.mkdir(exist_ok=True)
source = folder / 'contact.bin'
source.write_bytes(b'RFI6' + struct.pack('<I', 48) +
                  struct.pack('<5f7I', 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0) * 900)
env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl', RF_REPLAY_TRIGGER_UID='5658',
           RF_REPLAY_SETUP_UID='5671')
run = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'),
    '--spawn-replay', str(root / 'Installed_Game'), str(source),
    str(folder / 'contact.ppm')], cwd=root, env=env, capture_output=True, text=True)
(folder / 'contact.log').write_text(run.stdout + run.stderr)
labels = ('TRIGGER_CONTACTS', 'SCRIPT_MOVE', 'SCRIPT_ATTACK', 'ENEMY_FIRE',
          'SCRIPT_ANIMATION', 'SWITCH_DETAIL')
rows = {label: next((line.split()[1:] for line in run.stdout.splitlines()
                    if line.startswith(label + ' ')), None) for label in labels}
report = dict(returncode=run.returncode, telemetry=rows,
    scope='Player placed inside trigger5658, Switch5671 fired at frame0, held Use; '
          'no actor repositioning or forced Goto/Attack. Diagnostic probe only; '
          'exit0 alone does not prove the rescue fired or completed.')
(folder / 'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
raise SystemExit(run.returncode)
