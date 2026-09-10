"""Explicit lower-door fixture; PC by default, --native adds stock64MiB XEMU."""
import argparse,hashlib,json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--native',action='store_true');args=p.parse_args()
folder=root/'artifacts/door-contact';folder.mkdir(exist_ok=True);source=folder/'inputs.bin'
source.write_bytes(struct.pack('<5fI',0,0,1,0,0,0)*180)
env=dict(os.environ)
for key in ('RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START'):env.pop(key,None)
env['RF_REPLAY_DOOR_START']='1';exe=root/'build/pc/Release/rf_pc_play.exe'
run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
def row(name):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(name+' ')).split()[1:]))
sweeps=row('BODY_SWEEPS');contact=row('BODY_CONTACT');assert sweeps[2]>0 and sweeps[3]==0 and contact[17] in (0,1)
triggers=row('TRIGGER_CONTACTS');assert triggers[3]>0 and triggers[5]==0,triggers
activation=row('LIVE_ACTIVATION');assert activation[2]==2 and activation[5]==0 and activation[6]>0 and activation[6]==activation[7],activation
report=dict(live_activation=activation,trigger_contacts=triggers,result='PASS',frames=180,staged=True,body_sweeps=sweeps,contact=contact,
 mover_uid=[8544,8543][contact[17]],pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
 input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),scope='PC player collision with authored lower L1S1 door geometry from an explicit staged start. No route-from-spawn, door opening or moving-platform claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if args.native:subprocess.run([sys.executable,str(root/'tools/xemu_replay_check.py'),str(source),'--door','--seconds','180'],cwd=root,check=True)
