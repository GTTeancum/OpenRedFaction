"""Fight the two pistol guards covering the eastern chamber from its doorway.
Generate replay_area2_corridor.py first. Only ordinary process-local inputs.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-east-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area2-corridor-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+5350*48
records=bytearray(prefix)
for i in range(5350,5755):
    x,z=(.974,-.226) if i<5585 else ((.226,.974) if 5600<=i<5700 else ((-.974,.226) if i>=5720 else (0,0)))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,1,0,0,0,0))
for i in range(5755,6250):
    yaw=-.98 if i<5816 else (.98 if 5920<=i<5946 else 0)
    pitch=.05 if i<5816 else 0
    fire=int((5820<=i<5920 and (i-5820)%32==0) or (5950<=i<6050 and (i-5950)%32==0))
    records.extend(struct.pack('<5f7I',0,0,0,pitch,yaw,0,0,1,fire,int(i==6100),0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
combat,ammo,enemy=words('COMBAT'),words('PLAYER_AMMO'),words('ENEMY_COMBAT')
assert 'Completed 6250 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
assert combat[:3]==[37,26,6] and ammo[2:5]==[16,37,4]
rows=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')]
for uid in (8490,5677,5676,5678,8071,5683):
    assert float(next(r[-1] for r in rows if int(r[0])==uid))<=0
health=struct.unpack('<f',struct.pack('<I',enemy[5]))[0];assert health>0
report=dict(result='PASS',frames=6250,combat=combat,ammo=ammo,health=health,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Uninterrupted authored spawn through six player kills; full exit traversal remains unverified.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
