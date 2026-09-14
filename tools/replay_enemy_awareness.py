"""Unprovoked hostile encounter and passive neutral area through live scene input."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/awareness';folder.mkdir(exist_ok=True);rows=[]
for name,uid in [('hostile',8456),('neutral_miner',8431)]:
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+bytes(40*240))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID=str(uid),RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
 sight=words('ENEMY_AWARENESS');enemy=words('ENEMY_COMBAT');combat=words('COMBAT');health=struct.unpack('<f',struct.pack('<I',enemy[5]))[0]
 assert sight[7]==enemy[7]==combat[7]==0 and combat[0]==0 and sight[5]>0
 if name=='hostile':assert sight[1]==2 and enemy[2:4]==[8,8] and 0<health<100
 else:assert sight[1]==0 and enemy[2:4]==[0,0] and health==100
 rows.append(dict(case=name,uid=uid,awareness=sight,enemy=enemy,health=health));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Live unprovoked hostile acquisition, multiple attackers, neutral filtering and range rejection. Cone and obstruction rejection branches exist but are not separately demonstrated by these routes. No pursuit, squads or full scripted allegiance claim.'),indent=2))
