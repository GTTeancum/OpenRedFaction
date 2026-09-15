"""Fight after the full-spawn rescue, retreat behind the doorway, and reload.
Run replay_area2_spawn.py first to generate the validated 3000-frame prefix.
"""
import json,os,struct,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/cover-combat-replay';folder.mkdir(exist_ok=True)
prefix=(root/'artifacts/area2-spawn-replay/input.bin').read_bytes()
assert len(prefix)==8+3000*48 and prefix[:8]==b'RFI6'+struct.pack('<I',48)
records=bytearray(prefix[8:])
for i in range(3000,3600):
    x,z=(.1066,.9943) if i<3050 else ((-.48279,-.875735) if 3340<=i<3390 else (0,0))
    pitch,yaw=(-.1,-.9925) if 3050<=i<3074 else (0,0)
    fire=int(3080<=i<3340 and (i-3080)%32==0)
    records.extend(struct.pack('<5f7I',x,0,z,pitch,yaw,0,0,0,fire,int(i==3420),0,0))
source=folder/'input.bin';source.write_bytes(prefix[:8]+records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
combat,ammo,enemy=words('COMBAT'),words('PLAYER_AMMO'),words('ENEMY_COMBAT')
guard=int(next(l.split()[2] for l in run.stdout.splitlines() if l.startswith('NPC_BACKLINK_ROW 8490 ')))
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert 'Completed 3600 frames' in run.stdout
assert combat[:3]==[9,4,1] and combat[3]==guard
assert ammo[2:5]==[16,9,1] and words('PLAYER_LIFE')[0]==0
assert 0<struct.unpack('<f',struct.pack('<I',enemy[5]))[0]<100
assert 24<position[0]<25 and 9.5<position[2]<10.7
report=dict(result='PASS',combat=combat,ammo=ammo,enemy=enemy,position=position,
    prefix_sha256=hashlib.sha256(prefix).hexdigest(),
    scope='Full-spawn rescue prefix, then movement, aiming, nine semiautomatic shots, retreat and reload. Kills guard8490 and survives under cover; does not clear the remaining corridor or section exit.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
