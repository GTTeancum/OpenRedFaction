"""Walk into real exit volumes; verify translated arrival and continued movement."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/walk-exit';folder.mkdir(exist_ok=True);rows=[]
source=folder/'walk.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+b''.join(struct.pack('<5f6I',0,0,float(i>=30),0,0,0,0,0,0,0,0) for i in range(180)))
for name,level,uid,target,delta in [('forward','L1S1.rfl',9019,'L1S2.rfl',(-144,-32,-48)),('backward','L1S2.rfl',9346,'L1S1.rfl',(144,32,48)),('section3-forward','L1S2.rfl',9512,'L1S3.rfl',(41,32,-118)),('section3-backward','L1S3.rfl',9324,'L1S2.rfl',(-41,-32,118))]:
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env.update(RF_REPLAY_LEVEL=level,RF_REPLAY_EXIT_START=str(uid))
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
 def lines(prefix):return [l.split()[1:] for l in run.stdout.splitlines() if l.startswith(prefix+' ')]
 transitions=lines('LEVEL_TRANSITION');assert len(transitions)==1 and transitions[0][:3]==[level,target,str(uid)],transitions
 assert 30<int(transitions[0][3])<120 and 'Completed 180 frames' in run.stdout
 pose=list(map(float,lines('LEVEL_EXIT_POSE')[0]));arrival=list(map(float,lines('LEVEL_ARRIVAL')[0]))
 assert all(abs(arrival[i]-(pose[i]+delta[i]))<.00005 for i in range(3)),(pose,arrival)
 spawn=list(map(int,lines('PLAYER_SPAWN')[0]));facing=struct.unpack('<9f',struct.pack('<9I',*spawn[4:13]))
 assert all(abs(facing[i]-pose[3+i])<.000001 for i in range(9)),'arrival facing changed'
 body=list(map(int,lines('PC_PLAY_BODY')[0]));position=struct.unpack('<3f',struct.pack('<3I',*body[22:25]))
 assert sum((position[i]-arrival[i])**2 for i in range(3))>1,'movement did not continue after arrival'
 rows.append(dict(case=name,transition=transitions[0],departing=pose,arrival=arrival,final_position=position));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows),indent=2))
