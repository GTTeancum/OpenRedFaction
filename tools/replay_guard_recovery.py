"""Walk through the miner rescue, then into the released guard's sight."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/guard-recovery-replay';folder.mkdir(exist_ok=True)
source=folder/'input.bin'
def movement(frame):
    if 30<=frame<85:return 0,-1
    if 85<=frame<130 or 1200<=frame<1260:return 1,0
    if 1260<=frame<1440:return 0,1
    return 0,0
source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
    struct.pack('<5f7I',movement(i)[0],0,movement(i)[1],0,0,0,0,int(i<1200),0,0,0,0)
    for i in range(2400)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRIGGER_UID='5670')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',
    str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
attack,recovery,combat=words('SCRIPT_ATTACK'),words('ATTACK_RECOVERY'),words('ENEMY_COMBAT')
death=next(l.split() for l in run.stdout.splitlines() if l.startswith('DEATH_WATCH 8611 '))
assert 'Completed 2400 frames' in run.stdout
assert attack[1:3]==[8496,8490] and attack[4]==0 and attack[5]>0
assert recovery[0:2]==[1,1] and recovery[2]>0
assert int(death[2])==1 and recovery[3]*1000/60>int(death[3])
assert combat[6]==1, 'Unopposed guards should kill the exposed player'
report=dict(result='PASS',attack=attack,recovery=recovery,combat=combat,
    scope='One placement at trigger5670; subsequent movement and Use only. Guard8490 releases the dead miner, acquires the player, and fires. Local encounter, not full level traversal.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
