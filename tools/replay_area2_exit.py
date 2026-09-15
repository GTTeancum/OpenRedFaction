"""Walk from the six-kill Area2 route through the crouched exit into L2S3.
Run replay_area2_east.py first; no placement or event injection is used.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-exit-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area2-east-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+6250*48
records=bytearray(prefix)
for i in range(6250,6660):
    x,z=(.01273,.99985) if i<6320 else ((-.698,.716) if 6340<=i<6555 or i>=6625 else ((.716,.698) if 6575<=i<6602 else (0,0)))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,1,0,0,0,0))
for i in range(6660,7450):
    x,z=(-.698,.716) if i<6718 or 6810<=i<7010 or 7150<=i<7290 else ((.716,.698) if 6740<=i<6790 or 7030<=i<7130 else (0,0))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,int(i>=6822),int(i==6810),1,0,0,0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
assert 'Completed 7450 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
ammo=words('PLAYER_AMMO');assert ammo[:3]==[3,88,16]
health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];prefix_result=json.loads((root/'artifacts/area2-east-replay/report.json').read_text());assert prefix_result['result']=='PASS' and prefix_result['frames']==6250 and prefix_result['combat'][2]==6
assert health==prefix_result['health'] and 0<health<100
assert 'TAKEN_PICKUP l2s2a.rfl 8553' in run.stdout
report=dict(result='PASS',frames=7450,transitions=transitions,health=health,ammo=ammo,prefix_sha256=hashlib.sha256(prefix).hexdigest(),input_sha256=hashlib.sha256(records).hexdigest(),scope='Uninterrupted L2S2a spawn/rescue/combat prefix and natural crouched exit to L2S3; selected retained player state checked. Does not prove the whole campaign or retail parity.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
