"""First-pass player death gating and in-place respawn through recorded input."""
import os,struct,subprocess,json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/player-life';folder.mkdir(exist_ok=True);rows=[]
for name,frames in [('dead',1350),('held',1350),('respawn',1301),('resume',1500)]:
 def command(i):
  move=1 if (name=='dead' and i>1220) or (name=='resume' and i>1340) else 0
  use=int(i>=1190) if name=='held' else int(name in ('respawn','resume') and i==1300)
  fire=int(i==30 or (name=='resume' and i==1320))
  return struct.pack('<5f5I',move,0,0,0,0,0,0,use,fire,0)
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(command(i) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for k in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(k,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def row(label):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(label+' ')).split()[1:]))
 life=row('PLAYER_LIFE');enemy=row('ENEMY_COMBAT');combat=row('COMBAT');body=row('PC_PLAY_BODY');health=struct.unpack('<f',struct.pack('<I',enemy[5]))[0]
 assert life[0]==1 and life[7]==enemy[7]==combat[7]==0
 if name in ('dead','held'):assert life[1:3]==[0,1] and life[6]>0 and health<=0
 if name=='dead':
  ring=row('ACTOR_PLAYER_INPUT');assert all(ring[i:i+6]==[0]*6 for i in range(1,len(ring),7))
  image=Image.open(folder/(name+'.ppm'));image.save(folder/'dead.png');assert image.getpixel((225,185))==(16,16,16)
 if name in ('respawn','resume'):assert life[1:3]==[1,0] and life[4]==1300 and health>0
 if name=='respawn':assert health==100 and combat[5:7]==[12,0]
 if name=='resume':
  assert combat[0]==2
  ring=row('ACTOR_PLAYER_INPUT');assert all(ring[i]==1065353216 for i in range(1,len(ring),7))
 rows.append(dict(case=name,frames=frames,life=life,health=health,combat=combat,position=struct.unpack('<3f',struct.pack('<3I',*body[22:25]))));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Default100-health death from armed retaliation; blocked controls, fresh-use respawn, restored health/ammo and resumed firing and accepted movement input (horizontal displacement not demonstrated on this staged route). In-place starting-player restore; world/NPC/mission state persists. Not a full level reload/checkpoint.'),indent=2))
