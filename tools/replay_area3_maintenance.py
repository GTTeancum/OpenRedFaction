"""Cross the maintenance door and fight the second L2S3 guard.
Requires replay_area3_entry.py; only ordinary input after the full Area2 prefix.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area3-maintenance-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area3-entry-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+8000*48
records=bytearray(prefix)
for i in range(8000,8600):
    x,z=(-.497,-.868) if i<8014 else ((.868,-.497) if 8035<=i<8130 or 8160<=i<8300 else ((.497,.868) if 8320<=i<8370 else (0,0)))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,1,0,0,0,0))
for i in range(8600,9000):
    x,z=(.868,-.497) if i<8612 else ((.288,.958) if 8670<=i<8690 else (0,0))
    yaw=.98 if 8630<=i<8644 else 0;pitch=.98 if 8630<=i<8660 else 0
    fire=int(8693<=i<8890 and (i-8693)%32==0)
    records.extend(struct.pack('<5f7I',x,0,z,pitch,yaw,0,0,1,fire,int(i==8910),0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
assert 'Completed 9000 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
combat,ammo=words('COMBAT'),words('PLAYER_AMMO');assert combat[:3]==[14,8,2] and ammo[2:5]==[16,14,2]
rows={int(l.split()[1]):l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
assert float(rows[2020][-1])<=0 and float(rows[2047][-1])<=0 and float(rows[2061][-1])==100
assert float(rows[2043][-1])<=0 and float(rows[2058][-1])<=0
health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health>0
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert 72.5<position[0]<74 and 84<position[2]<85
report=dict(result='PASS',frames=9000,health=health,position=position,combat=combat,ammo=ammo,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Uninterrupted Area2 prefix and two L2S3 guards; friendly miner2061 remains undamaged; authored timed Slay victims2043/2058 are dead. Remaining encounters and exit open.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
