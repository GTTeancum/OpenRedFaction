"""Acquire authored L4S5 rifle and exercise process-local weapon cycling."""
import json,os,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/weapon-select';folder.mkdir(exist_ok=True);rows=[]
for name in [x for x in ('rifle','held_cycle','switch_back','cancel_burst','empty_reload','unowned','respawn') if len(sys.argv)==1 or x in sys.argv[1:]]:
 def record(i):
  if name=='respawn':return struct.pack('<5f6I',0,0,float(10<=i<25),0,0,0,0,int(i==1600),int(i==1680),int(i==1700),int(i in (40,1650)))
  cycle=(40<=i<90) if name=='held_cycle' else i==40 or (name=='switch_back' and i==90) or (name=='cancel_burst' and i==65)
  return struct.pack('<5f6I',0,0,float(10<=i<25),0,0,0,0,0,int(i==60),int(name=='empty_reload' and i==90),int(cycle))
 source=folder/(name+'.bin');source.write_bytes(b'RFI5'+struct.pack('<I',44)+b''.join(record(i) for i in range(1800 if name=='respawn' else 120)))
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_LEVEL='L1S1.rfl' if name=='unowned' else 'L4S5.rfl',RF_REPLAY_ITEM_UID='9427' if name=='unowned' else '3415',RF_REPLAY_ARCHIVE='levels1.vpp')
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(x for x in run.stdout.splitlines() if x.startswith(key+' ')).split()[1:]))
 selection,ammo,combat,pickups,audio=map(words,('WEAPON_SELECTION','PLAYER_AMMO','COMBAT','PICKUPS','WEAPON_AUDIO'))
 npc_sounds=words('ENEMY_FIRE')[3]
 assert ammo[7]==combat[7]==pickups[7]==audio[7]==0
 if name=='respawn':
  assert words('PLAYER_LIFE')[:2]==[1,1] and selection[:4]==[1,2,8,1] and ammo[:5]==[8,197,42,3,1],(selection,ammo)
  assert combat[0]==3 and audio[0]==4+npc_sounds and audio[2]==4+npc_sounds and pickups[3]==1
 # L1S1 strips starting inventory before the staged handgun pickup; no spare ammo survives.
 elif name=='unowned':assert selection[:2]==[0,0] and ammo[:3]==[3,0,15] and combat[0]==1
 else:
  shots=1 if name=='cancel_burst' else 3
  back=name in ('switch_back','cancel_burst')
  assert pickups[3:6]==[1,42,3415] and combat[0]==shots,(pickups,combat)
  assert selection[:4]==[0 if back else 1,2 if back else 1,8,1] and selection[4]==42-shots,selection
  assert ammo[:3]==([3,125,16] if back else [8,0,42-shots]),ammo
  assert audio[0]==shots+npc_sounds and audio[2]==shots+npc_sounds,(audio,npc_sounds)
 rows.append(dict(name=name,selection=selection,ammo=ammo,combat=combat,pickups=pickups));print(rows[-1],flush=True)
(folder/('report.json' if len(sys.argv)==1 else '-'.join(sys.argv[1:])+'-report.json')).write_text(json.dumps(dict(result='PASS',cases=rows),indent=2))
