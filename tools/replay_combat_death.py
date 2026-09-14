"""Live hitscan death holds its final pose instead of resuming the standing controller."""
import os,struct,subprocess,json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/combat-death';folder.mkdir(exist_ok=True);rows=[]
for frames in (360,480):
 source=folder/(str(frames)+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(i>=30 and i%30==0),0) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='8456',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(str(frames)+'.ppm'))],env=env,capture_output=True,text=True)
 (folder/(str(frames)+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 def words(key):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(key+' ')).split()[1:]))
 death=words('COMBAT_DEATH');combat=words('COMBAT');assert combat[2]==1 and combat[7]==0
 assert death[:4]==[1,5,62,0] and death[4:7]==[12800,1065353216,1]
 Image.open(folder/(str(frames)+'.ppm')).save(folder/(str(frames)+'.png'))
 rows.append(dict(frames=frames,death=death,combat=combat));print(rows[-1],flush=True)
assert rows[0]['death']==rows[1]['death']
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Visible guard8456 death from player fire; generic action completes and remains frozen at360/480 frames. Rendered capture inspection accompanies state evidence. Full corpse physics, other death variants and death audio excluded.'),indent=2))
