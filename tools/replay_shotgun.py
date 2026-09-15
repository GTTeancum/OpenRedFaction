"""Acquire the authored L2S3 shotgun and exercise both firing modes.
One process-local starting placement; no inventory or health injection.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/shotgun-gameplay';folder.mkdir(parents=True,exist_ok=True)
source=folder/'input.bin'
source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
    struct.pack('<5f7I',0,0,float(10<=i<25),0,0,0,0,0,int(i==60),int(i==250),int(i==40),int(160<=i<205))
    for i in range(360)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S3.rfl',RF_REPLAY_ITEM_UID='2115',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
shotgun,ammo,combat,pickups,selection=map(words,['SHOTGUN','PLAYER_AMMO','COMBAT','PICKUPS','WEAPON_SELECTION'])
assert shotgun[:5]==[5,20,5,1,4] and shotgun[6:]==[0,0]
assert ammo[:5]==[5,0,3,0,0] and ammo[7]==0
assert combat[:3]==[5,5,1] and pickups[3:6]==[1,8,2115] and selection[:2]==[3,1]
assert words('PLAYER_LIFE')[0]==0 and words('PLAYER_WEAPON')[2]>0 and words('PLAYER_WEAPON')[6]==0
assert words('RIOT_STICK')==[0]*8
rows={int(l.split()[1]):l.split()[1:] for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
assert float(rows[2114][-1])<=0 and float(rows[2061][-1])==100
report=dict(result='PASS',frames=360,shotgun=shotgun,ammo=ammo,combat=combat,scope='Staged near authored item2115; normal pickup, cycling, one primary and four alternate shots kill guard2114; reload without reserve grants no ammo. No full-route, reload-transfer or level-persistence claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
