"""Native PC renderer captures validate shared combat HUD states."""
import json,os,struct,subprocess
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/combat-hud';folder.mkdir(exist_ok=True)
rows=[]
for name,frames in [('ready',30),('hit',34),('reloading',90),('hidden',34)]:
 source=folder/(name+'.bin');source.write_bytes(b'RFI4'+struct.pack('<I',40)+b''.join(struct.pack('<5f5I',0,0,0,0,0,0,0,0,int(i>=30),int(i==60)) for i in range(frames)))
 env=dict(os.environ,RF_REPLAY_ACTOR_UID='9858' if name=='hidden' else '8456',RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
 for key in ('RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION','RF_REPLAY_LIGHTMAP_REGEN','RF_REPLAY_DOOR_START','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 ppm=folder/(name+'.ppm')
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(ppm)],env=env,capture_output=True,text=True)
 (folder/(name+'.txt')).write_text(run.stdout+run.stderr);run.check_returncode()
 state=list(map(int,next(line for line in run.stdout.splitlines() if line.startswith('COMBAT ')).split()[1:]));assert state[7]==0
 image=Image.open(ppm);image.save(folder/(name+'.png'))
 if name=='hidden':assert state[0]==1 and state[1]==0
 reticle=(96,255,128) if name=='hit' else (238,238,238)
 assert image.getpixel((312,240))==reticle,(name,image.getpixel((312,240)))
 for i in range(12):assert image.getpixel((480+i*11,448))==((238,238,238) if i<state[5] else (72,72,72)),(name,i)
 if name=='reloading':assert state[6]>0 and image.getpixel((480,459))==(255,192,64) and image.getpixel((600,459))==(72,72,72)
 rows.append(dict(case=name,frames=frames,combat=state));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='PC native renderer pixels: ready reticle, brief damaging-hit confirmation, twelve clip markers and reload progress. No desktop capture/input. Xbox uses the same overlay producer; native Xbox validation separate.'),indent=2))
