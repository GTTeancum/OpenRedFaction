"""Continue the Area2 exit through the first L2S3 guard using ordinary input.
Run replay_area2_exit.py first; no placement, health or event injection.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area3-entry-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area2-exit-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+7450*48
records=bytearray(prefix)
for i in range(7450,8000):
    x,z=(-.698,.716) if i<7500 else ((.5,.866) if 7565<=i<7665 else (0,0))
    yaw=.98 if 7520<=i<7537 else 0;pitch=-.98 if 7520<=i<7552 else 0
    fire=int(7655<=i<7850 and (i-7655)%30==0)
    records.extend(struct.pack('<5f7I',x,0,z,pitch,yaw,1,0,1,fire,int(i==7900),0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
assert 'Completed 8000 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
combat,ammo=words('COMBAT'),words('PLAYER_AMMO');assert combat[:3]==[7,4,1] and ammo[2:5]==[16,7,1]
guard=next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW 2020 '))
assert float(guard[-1])<=0
health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert 0<health<100
report=dict(result='PASS',frames=8000,transitions=transitions,combat=combat,ammo=ammo,health=health,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Full Area2 combat/exit prefix then first L2S3 guard kill and reload; remaining L2S3 traversal open.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
