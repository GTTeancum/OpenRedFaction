"""Compare active-range and full-capacity scratch initialization with visible actors."""
import hashlib,json,os,struct,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];d=r/'artifacts/scratch-differential';d.mkdir(exist_ok=True)
source=d/'input.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+bytes(120*44));runs=[]
for full in (False,True):
 name='full' if full else 'active';env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ACTOR_UID='8323')
 if full:env['RF_REPLAY_FULL_SCRATCH']='1'
 run=subprocess.run([str(r/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(r/'Installed_Game'),str(source),str(d/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (d/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
 wanted=('NPC_DRAW ','CLUTTER_DRAW ','WEAPON_DRAW ','PLAYER_WEAPON ')
 rows={p.strip():next(x for x in run.stdout.splitlines() if x.startswith(p)) for p in wanted}
 assert int(rows['NPC_DRAW'].split()[3])>0,rows
 runs.append(rows)
assert runs[0]==runs[1],runs
assert (d/'active.ppm').read_bytes()==(d/'full.ppm').read_bytes(),'pixels differ'
report=dict(result='PASS',pc_sha256=hashlib.sha256((r/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(),scope='Same current binary, visible actor8323; full vs active scratch initialization, identical final pixels/draw summaries.',draws=runs[0])
(d/'report.json').write_text(json.dumps(report,indent=2));print(report)
