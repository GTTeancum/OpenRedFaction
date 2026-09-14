"""Recorded gameplay drives first-person pistol poses; native captures are inspected separately."""
import os,struct,subprocess,json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/player-weapon-live';folder.mkdir(exist_ok=True);rows=[]
for name,frames,fire,reload,clip in [('idle',10,-1,-1,0),('fire',35,30,-1,1),('reload',80,30,60,2),('returned',150,30,60,0)]:
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(i==fire),int(i==reload)) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8431',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
 weapon=words('PLAYER_WEAPON');combat=words('COMBAT')
 assert weapon[0]==frames and weapon[1]==clip and weapon[2]>0 and weapon[3]<=weapon[4]<=1024*1024 and weapon[6]==0,weapon
 Image.open(folder/(name+'.ppm')).save(folder/(name+'.png'))
 rows.append(dict(name=name,frames=frames,weapon=weapon,combat=combat));print(rows[-1],flush=True)
assert len({row['weapon'][5] for row in rows[:3]})==3
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Live first-person idle, firing, reload and return poses; bounded owner and emitted geometry. Screenshots require visual inspection. Provisional placement, timing and depth band; full weapon selection is pending.'),indent=2))
