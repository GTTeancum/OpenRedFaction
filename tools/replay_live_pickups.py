"""Authored Handgun9427 rendering, full inventory rejection and one-time collection."""
import os,struct,subprocess,json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/live-pickups';folder.mkdir(exist_ok=True);rows=[]
for name,frames in [('view',1),('full',210),('collect',210),('once',400),('magazine',660)]:
 def inputs(i):
  move=int((600<=i<615) if name=='magazine' else (130<=i<145))
  fire=int((i>=30 and i<=480 and i%30==0) if name=='magazine' else (name in ('collect','once') and (i==30 or (name=='once' and i==260))))
  reload=int(i==510 if name=='magazine' else (name in ('collect','once') and (i==60 or (name=='once' and i==290))))
  return struct.pack('<5f5I',0,0,move,0,0,0,0,0,fire,reload)
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(inputs(i) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ITEM_UID='9427',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for k in ('RF_REPLAY_ACTOR_UID','RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(k,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
 pickups=words('PICKUPS');ammo=words('PLAYER_AMMO')
 assert pickups[0]==8 and pickups[7]==ammo[7]==0
 if name in ('view','full'):assert pickups[3:5]==[0,0] and pickups[6]>0 and ammo[1:3]==[125,16],(pickups,ammo)
 else:assert pickups[3]==1 and pickups[4]==(16 if name=='magazine' else 1) and pickups[5]==9427 and pickups[6]==0 and ammo[1:3]==([124,16] if name=='once' else [125,16]),(pickups,ammo)
 Image.open(folder/(name+'.ppm')).save(folder/(name+'.png'))
 rows.append(dict(name=name,pickups=pickups,ammo=ammo));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Live Handgun9427, authored position/model/quantity; full inventory leaves item, partial/full-magazine reserve grants and one-time disappearance. Other classes, pickup sounds and mission event publication pending.'),indent=2))
