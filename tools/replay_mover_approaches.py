"""Reproducible PC approaches from authored L1S1 spawn; never asserts an unreached mover."""
import argparse,hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
forward=(1200,0,1);lower_b=[forward,(450,-.7822154,.6230081)]
routes={'forward':[(480,0,1)],'forward-long':[forward],'right':[(1200,1,0)],
 'diagonal':[(1200,.70710677,.70710677)],'lower-door-a':[forward,(450,.7822154,.6230081)],
 'lower-door-b':lower_b,'door-direct':lower_b+[(600,.9,.4)],'door-straight':lower_b+[(600,.7822154,.6230081)]}
p=argparse.ArgumentParser();p.add_argument('--case',choices=routes,default='door-direct');args=p.parse_args()
folder=root/'artifacts/mover-approach';folder.mkdir(exist_ok=True)
source=folder/(args.case+'.bin');payload=b''.join(struct.pack('<5fI',side,0,front,0,0,0)*frames for frames,side,front in routes[args.case]);source.write_bytes(payload)
env=dict(os.environ)
for key in ('RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START'):env.pop(key,None)
exe=root/'build/pc/Release/rf_pc_play.exe'
run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(args.case+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
(folder/(args.case+'.txt')).write_text(run.stdout+run.stderr)
if run.returncode:raise RuntimeError(f'PC replay failed ({run.returncode}): {run.stderr}')
def row(name):return list(map(int,next(line for line in run.stdout.splitlines() if line.startswith(name+' ')).split()[1:]))
body=row('BODY_SWEEPS');ground=row('GROUND_QUERIES');state=row('PC_PLAY_BODY')
assert body[3]==ground[3]==0
report=dict(result='MOVER_CONTACT' if body[2] or ground[2] else 'MOVER_NOT_REACHED',case=args.case,
 frames=len(payload)//24,input_sha256=hashlib.sha256(payload).hexdigest(),pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
 body_sweeps=body,ground_queries=ground,position=struct.unpack('<3f',struct.pack('<3I',*state[22:25])),segments=routes[args.case],
 scope='PC process-local commands from authored L1S1 spawn. Counts distinguish static from mover contacts. No XEMU result, route fidelity or door activation is inferred from successful replay execution.')
(folder/(args.case+'-report.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)
