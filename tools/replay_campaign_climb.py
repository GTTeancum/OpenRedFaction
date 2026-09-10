"""Stage the player at the first authored region for a headless climb replay.
This changes only the targeted process environment and its replay input.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/climb-live';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin';source.write_bytes(b''.join(struct.pack('<5fI',0,0,1 if i>=8 else 0,0,0,0) for i in range(120)))
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'final.ppm')],
 cwd=root,env=dict(os.environ,RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_REGION_START='1'),capture_output=True,text=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
report=dict(result='FAIL',level='L1S2.rfl',staged_start='First authored region center',frames=120,exit_code=run.returncode)
if run.returncode:report['error']=run.stderr
else:
 row=next((l for l in run.stdout.splitlines() if l.startswith('PLAYER_CLIMB ')),None)
 if row:
  report['climb']=list(map(int,row.split()[1:]));report['result']='PASS' if report['climb'][1] and report['climb'][2] else 'INCOMPLETE'
report['scope']='Staged process-local campaign replay; PASS requires entry and exit counters, not a visual fidelity or full campaign claim.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='PASS' else 1)
