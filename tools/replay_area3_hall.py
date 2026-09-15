"""Fight the third L2S3 guard after the uninterrupted maintenance replay.
Only ordinary movement, aim, Use, fire and reload; no staging or health grants.
"""
import hashlib,json,math,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area3-hall-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area3-maintenance-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+9000*48
records=bytearray(prefix);yaw=math.atan2(-.286234,.953973)
for i in range(9000,9600):
    turn=.98 if i<9204 else 0
    x,z=(math.cos(yaw),math.sin(yaw)) if i<9254 else (0,0)
    records.extend(struct.pack('<5f7I',x,0,z,-.98 if i<9008 else 0,turn,0,0,1,int(9255<=i<9380 and (i-9255)%30==0),int(i==9430),0,0))
    yaw+=turn/60
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
assert 'Completed 9600 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
combat,ammo=words('COMBAT'),words('PLAYER_AMMO');assert combat[:3]==[19,13,3] and ammo[2:5]==[16,19,3]
rows={int(l.split()[1]):l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
assert all(float(rows[uid][-1])<=0 for uid in [2020,2047,1751,2043,2058])
assert float(rows[2061][-1])==100
health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health>0
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert 98<position[0]<99 and 83.5<position[2]<84.5
report=dict(result='PASS',frames=9600,health=health,position=position,combat=combat,ammo=ammo,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Uninterrupted Area2 prefix and three L2S3 guard kills; friendly2061 unharmed and authored Slay victims dead. Elevator and exit remain open.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
