"""Look-directed climb using only commands reachable from the two controller sticks.
Starts in the existing first-region fixture; does not simulate host input or prove
an authored approach/top-platform exit. Lower the view while moving to exit sideways.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/climb-controls';out.mkdir(exist_ok=True)
source=out/'inputs.bin';frames=180
commands=[(0,0,1 if i>=80 else 0,1 if i<80 else -1 if i>=105 else 0,0,0) for i in range(frames)]
assert all(c[1]==0 and all(-1<=v<=1 for v in c[:5]) for c in commands)
source.write_bytes(b''.join(struct.pack('<5fI',*c) for c in commands))
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(out/'final.ppm')],cwd=root,env=dict(os.environ,RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_REGION_START='1'),capture_output=True,text=True)
(out/'pc.txt').write_text(run.stdout+run.stderr)
report=dict(result='FAIL',frames=frames,vertical_command=0,scope='Staged start, stick-reachable look/forward commands; no controller device injection or top-platform fidelity claim')
if run.returncode:report['error']=run.stderr
else:
 rows={l.split()[0]:list(map(int,l.split()[1:])) for l in run.stdout.splitlines() if l.startswith(('PLAYER_CLIMB ','PLAYER_CLIMB_FRAMES '))}
 v=rows['PLAYER_CLIMB_FRAMES'];timeline=sorted([v[i:i+9] for i in range(0,len(v),9) if v[i]])
 heights=[struct.unpack('<f',struct.pack('<I',r[4]))[0] for r in timeline if r[2]==2]
 report.update(climb=rows['PLAYER_CLIMB'],retained_climbing_ticks=len(heights),vertical_distance=max(heights)-min(heights) if heights else 0)
 report['result']='PASS' if report['climb'][1:3]==[1,1] and report['vertical_distance']>3 else 'INCOMPLETE'
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='PASS' else 1)
