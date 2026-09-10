"""Stage the player at the first authored region for a headless climb replay.
This changes only the targeted process environment and its replay input.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/climb-live';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin';source.write_bytes(b''.join(struct.pack('<5fI',0,1 if 8<=i<53 else 0,1 if i>=53 else 0,0,0,0) for i in range(120)))
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'final.ppm')],
 cwd=root,env=dict(os.environ,RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_REGION_START='1'),capture_output=True,text=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
report=dict(result='FAIL',level='L1S2.rfl',staged_start='First authored region center',frames=120,exit_code=run.returncode)
if run.returncode:report['error']=run.stderr
else:
 row=next((l for l in run.stdout.splitlines() if l.startswith('PLAYER_CLIMB ')),None)
 if row:
  report['climb']=list(map(int,row.split()[1:]))
  values=list(map(int,next(l for l in run.stdout.splitlines() if l.startswith('PLAYER_CLIMB_FRAMES ')).split()[1:]))
  timeline=[values[i:i+9] for i in range(0,len(values),9) if values[i]]
  climbing=[r for r in timeline if r[2]==2]
  heights=[struct.unpack('<f',struct.pack('<I',r[4]))[0] for r in climbing]
  report['climbing_ticks']=len(climbing);report['vertical_distance']=max(heights)-min(heights) if heights else 0
  report['result']='PASS' if report['climb'][1] and report['climb'][2] and report['vertical_distance']>1 else 'INCOMPLETE'
report['scope']='Staged process-local campaign replay; Parent-axis climb input followed by sideways exit; requires transitions and >1 world unit ascent. Does not prove controller mapping, top-of-ladder traversal or full campaign fidelity.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='PASS' else 1)
