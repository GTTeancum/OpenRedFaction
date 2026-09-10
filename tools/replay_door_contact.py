"""Explicit lower-door fixture; PC by default, --native adds stock64MiB XEMU."""
import argparse,hashlib,json,os,struct,subprocess,sys
from pathlib import Path
from door_fixture_metrics import measure
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--native',action='store_true');p.add_argument('--idle',action='store_true');p.add_argument('--cycle',action='store_true');p.add_argument('--frames',type=int,default=180);args=p.parse_args();assert args.frames>0
if args.cycle:
 if args.idle:p.error('--cycle and --idle are exclusive')
 args.frames=420
folder=root/'artifacts/door-contact';folder.mkdir(exist_ok=True);source=folder/'inputs.bin'
segments=[(180,1),(30,0),(60,-1),(150,0)] if args.cycle else [(args.frames,0 if args.idle else 1)]
source.write_bytes(b''.join(struct.pack('<5fI',0,0,front,0,0,0)*count for count,front in segments))
env=dict(os.environ)
for key in ('RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START'):env.pop(key,None)
env['RF_REPLAY_DOOR_START']='1';exe=root/'build/pc/Release/rf_pc_play.exe'
run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
def row(name):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(name+' ')).split()[1:]))
sweeps=row('BODY_SWEEPS');contact=row('BODY_CONTACT');assert sweeps[3]==0
traversal=measure(row('PLAYER_SPAWN'),row('PC_PLAY_BODY'))
if not args.idle and args.frames>=180:assert traversal['crossed'],traversal
triggers=row('TRIGGER_CONTACTS');assert triggers[3]>0 and triggers[5]==0,triggers
activation=row('LIVE_ACTIVATION');assert activation[2]==2 and activation[5]==0 and activation[6]>0 and activation[6]==activation[7],activation
motion=row('LIVE_MOTION');positions=row('DOOR_POSITIONS');assert motion[7]==0 and motion[4]>=2,motion
if args.idle and args.frames>=300:assert motion[2]>0,motion
if args.cycle:assert motion[3]==2 and motion[4]>=6,motion
if args.cycle or (args.idle and args.frames>=180) or args.frames==180:
 groups=next(l for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if l['file'].lower()=='l1s1.rfl')['records']
 expected_positions=[]
 for uid in (8593,8591):
  g=next(g for g in groups if g['keys'][0]['uid']==uid)
  expected_positions.extend(struct.unpack('<3I',struct.pack('<3f',*g['keys'][1]['position'])))
 assert positions==expected_positions,(positions,expected_positions)

report=dict(traversal=traversal,live_motion=motion,door_positions=positions,live_activation=activation,trigger_contacts=triggers,result='PASS',frames=args.frames,idle=args.idle,cycle=args.cycle,segments=segments,staged=True,body_sweeps=sweeps,contact=contact,
 mover_uid=([8544,8543][contact[17]] if contact[17] in (0,1) else None),pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
 input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),scope='PC player collision with authored lower L1S1 door geometry from an explicit staged start. Live door pose integration; no route-from-spawn or moving-platform carry claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if args.native:subprocess.run([sys.executable,str(root/'tools/xemu_replay_check.py'),str(source),'--door','--seconds','180'],cwd=root,check=True)
