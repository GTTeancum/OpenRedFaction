"""Playable-first armed retaliation checks through normal scene input."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/enemy-first';folder.mkdir(exist_ok=True)
rows=[]
for name,frames in [('idle',240),('provoke',240),('kill',360),('down',1500)]:
 source=folder/(name+'.bin')
 source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(name!='idle' and (i==30 or (name=='kill' and i>=30) or (name=='down' and i==1490))),0) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for k in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(k,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(label):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(label+' ')).split()[1:]))
 enemy=words('ENEMY_COMBAT');player=words('COMBAT');health=struct.unpack('<f',struct.pack('<I',enemy[5]))[0]
 assert enemy[0]==frames and enemy[7]==player[7]==0
 if name=='idle':assert enemy[1]==2 and enemy[2]==enemy[3]==8 and 0<health<100 and player[0]==0
 if name=='provoke':assert enemy[1]==2 and enemy[2]==enemy[3]==8 and 0<health<100 and player[:2]==[1,1]
 if name=='kill':assert player[2]==1 and enemy[2]==enemy[3]==8 and health>0
 if name=='down':assert enemy[6]==1 and health<=0 and player[0]==1
 rows.append(dict(case=name,frames=frames,enemy=enemy,player=player,health=health));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Nearby hostile armed NPCs acquire without provocation; killing one attacker reduces return fire while the other continues and zero player health prevents further firing. Retained damage/armor and HUD health. No patrol/pursuit, weapon-specific AI, cover transition or shooting-animation claim.'),indent=2))
