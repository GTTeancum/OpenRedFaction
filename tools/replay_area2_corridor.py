"""Continue the full-spawn medical route through the remaining melee guards.
Run replay_cover_combat.py --medical-crate first. No world state is injected.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-corridor-replay';folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/cover-combat-medical-replay/input.bin').read_bytes()
assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+4500*48
# Turn back toward the pursuers immediately after collecting the kit.
# The original fixture's final 300 idle frames are deliberately omitted.
records=bytearray(prefix[:8+4200*48])
for i in range(4200,4900):
    yaw=.98 if i<4253 else 0
    fire=int(4256<=i<4750 and (i-4256)%32==0)
    records.extend(struct.pack('<5f7I',0,0,0,0,yaw,0,0,0,fire,int(i==4780),0,0))
for i in range(4900,5350):
    x,z=(-.974,.226) if i<4910 else ((.226,.974) if 4930<=i<5130 or 5180<=i<5240 else ((.974,-.226) if 5140<=i<5165 else (0,0)))
    records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,1,0,0,0,0))
source=folder/'input.bin';source.write_bytes(records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
combat=words('COMBAT');ammo=words('PLAYER_AMMO');enemy=words('ENEMY_COMBAT')
assert 'Completed 5350 frames' in run.stdout and words('PLAYER_LIFE')[0]==0
assert combat[:3]==[29,18,4] and ammo[2:5]==[16,29,3]
assert 'TAKEN_PICKUP l2s2a.rfl 8553' in run.stdout
snapshots=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')]
for uid in (8490,5677,5676,5678):
    assert float(next(row[-1] for row in snapshots if int(row[0])==uid))<=0
health=struct.unpack('<f',struct.pack('<I',enemy[5]))[0];assert health>40
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert 26.5<position[0]<27.6 and 28<position[2]<29.2
report=dict(result='PASS',position=position,frames=5350,combat=combat,ammo=ammo,health=health,prefix_sha256=hashlib.sha256(prefix).hexdigest(),scope='Four player kills, medical recovery and traversal through the northern hall door from authored spawn; the section exit is still unverified.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
