"""Walk from the authored Area2 spawn through the miner rescue."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-spawn-replay';folder.mkdir(exist_ok=True)
def move(i):
    if 2700<=i<2707:return -.1066,-.9943
    if 2730<=i<2770:return .9943,-.1066
    if 2810<=i<2860:return .1066,.9943
    if 30<=i<300 or 460<=i<530:return 0,1
    if 360<=i<415 or 570<=i<740 or 960<=i<1110 or 1420<=i<1450:return 1,0
    if 900<=i<925 or 1180<=i<1230 or 1350<=i<1385:return 0,-1
    if 1260<=i<1315:return -1,0
    return 0,0
source=folder/'input.bin'
source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
    struct.pack('<5f7I',move(i)[0],0,move(i)[1],0,0,0,int(i==1260),int(i<2700),0,0,0,0) for i in range(3000)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
assert 'Completed 3000 frames' in run.stdout
traces=[list(map(float,l.split()[1:])) for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_TRACE ')]
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
attack,door=words('SCRIPT_ATTACK'),words('ROTATING_DOORS')
assert words('SWITCH_DETAIL')[0]==5671
assert door[1:3]==[1,8512] and attack[1:3]==[8496,8490] and attack[4]==0 and attack[5]>0
assert any(l.startswith('DEATH_WATCH 8611 1 ') for l in run.stdout.splitlines())
assert words('PLAYER_LIFE')[0]==0
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert 24<position[0]<25 and 9.5<position[2]<10.7, position
report=dict(status='PASS',traces=traces,attack=attack,door=door,
    scope='Authored L2S2a spawn; movement, one jump and Use only. Reaches and completes the miner rescue encounter, then walks through the opened cell door. No placement or event injection. Full section exit and combat beyond the rescue remain unverified.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report['scope']);print(attack)
