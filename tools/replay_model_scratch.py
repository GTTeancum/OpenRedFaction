"""Compare current shared model draws with a preserved pre-change replay."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('baseline',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1];base=a.baseline.resolve();d=r/'artifacts/scratch-active';d.mkdir(exist_ok=True)
exe=r/'build/pc/Release/rf_pc_play.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
reference=json.loads((base/'report.json').read_text());assert sha!=reference['pc_sha256'],'Executable unchanged from baseline'
e={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};e.update(RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_EXIT_START='9512')
run=subprocess.run([str(exe),'--spawn-replay',str(r/'Installed_Game'),str(r/'artifacts/walk-exit/walk.bin'),str(d/'result.ppm')],env=e,capture_output=True,text=True)
(d/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
assert (d/'result.ppm').read_bytes()==(base/'pc-final.ppm').read_bytes(),'pixels changed'
old=(base/'pc-reference.txt').read_text();checks={}
for prefix in ('NPC_DRAW ','CLUTTER_DRAW ','WEAPON_DRAW ','PLAYER_WEAPON '):
 before=[x for x in old.splitlines() if x.startswith(prefix)];after=[x for x in run.stdout.splitlines() if x.startswith(prefix)]
 assert before and before==after,(prefix,before,after);checks[prefix.strip()]=after
report=dict(result='PASS',pc_sha256=sha,baseline_pc_sha256=reference['pc_sha256'],pixels_identical=True,checks=checks)
(d/'report.json').write_text(json.dumps(report,indent=2));print(report)
