"""Revisit L1S1 through two actual walking-trigger transitions in one process."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/roundtrip';folder.mkdir(exist_ok=True)
source=folder/'walk.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+b''.join(struct.pack('<5f6I',0,0,0 if i<30 else 1 if i<120 else -1,0,0,0,0,0,0,0,0) for i in range(480)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_EXIT_START='9019')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'walk.ppm')],env=env,capture_output=True,text=True)
(folder/'walk.log').write_text(run.stdout+run.stderr);run.check_returncode()
def rows(key):return [line.split()[1:] for line in run.stdout.splitlines() if line.startswith(key+' ')]
transitions=rows('LEVEL_TRANSITION');assert transitions==[['L1S1.rfl','L1S2.rfl','9019','62'],['L1S2.rfl','L1S1.rfl','9346','264']],transitions
assert 'Completed 480 frames' in run.stdout
poses=[list(map(float,row)) for row in rows('LEVEL_EXIT_POSE')];arrivals=[list(map(float,row)) for row in rows('LEVEL_ARRIVAL')]
assert len(poses)==len(arrivals)==2
for pose,arrival,delta in zip(poses,arrivals,[(-144,-32,-48),(144,32,48)]):
 assert all(abs(arrival[i]-pose[i]-delta[i])<.00005 for i in range(3))
spawn=list(map(int,rows('PLAYER_SPAWN')[0]));facing=struct.unpack('<9f',struct.pack('<9I',*spawn[4:13]))
assert all(abs(facing[i]-poses[-1][3+i])<.000001 for i in range(9))
ammo=list(map(int,rows('PLAYER_AMMO')[0]));assert ammo[:3]==[3,125,16] and ammo[7]==0,ammo
report=dict(result='PASS',frames=480,transitions=transitions,departing=poses,arrivals=arrivals,ammo=ammo)
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
