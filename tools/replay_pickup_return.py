"""Collect and shoot a rifle, force authored exits, revisit the same item.

Process-local placement and exit dispatch isolate persistence from route finding.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/pickup-return';folder.mkdir(exist_ok=True)
source=folder/'inputs.bin'
source.write_bytes(b'RFI5'+struct.pack('<I',44)+b''.join(
    struct.pack('<5f6I',0,0,float(10<=i<25 or 191<=i<206),0,0,0,0,0,int(i==45),0,int(i==40)) for i in range(240)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L4S5.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_ITEM_UID='3415',RF_REPLAY_EXIT_UID='861',RF_REPLAY_RETURN_EXIT_UID='592')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'final.ppm')],env=env,capture_output=True,text=True)
(folder/'pc.log').write_text(run.stdout+run.stderr)
assert run.returncode==0,run.stderr[-1200:]
transitions=[x for x in run.stdout.splitlines() if x.startswith('LEVEL_TRANSITION ')]
assert transitions==['LEVEL_TRANSITION L4S5.rfl L4S4.rfl 861 61','LEVEL_TRANSITION L4S4.rfl L4S5.rfl 592 181'],transitions
def words(label):return list(map(int,next(x for x in run.stdout.splitlines() if x.startswith(label+' ')).split()[1:]))
ammo,pickups=words('PLAYER_AMMO'),words('PICKUPS')
assert ammo[:3]==[8,0,39] and pickups[3]==0,(ammo,pickups)
assert 'TAKEN_PICKUP l4s5.rfl 3415' in run.stdout.splitlines()
def floats(values):return struct.unpack('<'+'f'*len(values),struct.pack('<'+'I'*len(values),*values))
spawn=floats(words('PLAYER_SPAWN')[1:4]);body=floats(words('PC_PLAY_BODY')[22:25])
# Inverse of the documented stage_item offset, not a simulated collection.
item=(spawn[0],spawn[1]-.625,spawn[2]+2.5)
distance=sum((a-b)**2 for a,b in zip(item,body))**.5
assert distance<2,(body,item,distance)
report=dict(result='PASS',frames=240,transitions=transitions,ammo=ammo,pickups=pickups,return_item_distance=distance,
            scope='Actual pickup contact, rifle fire, authored exit dispatch and staged return; no played-route claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
