"""Inspect real saved beam joint cuts from elevated render-only views."""
import json,os,struct,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-cap-views';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
inputs=folder/'neutral.bin';inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(121*48))
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='95',RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(ROOT/'artifacts/xemu/render-20260917-191001/xbox-checkpoint.rfds'))
views={'baseline':None,'near-cap':'-3,2.25,0,-5,1.5,2.5','far-cap':'-3,2.25,0,-5,1.5,-2.5','far-reverse':'-4,2.25,-4,-5,1.5,-2.5'}
report={};baseline=None
for name,camera in views.items():
 checkpoint=folder/(name+'.rfcp');checkpoint.unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(checkpoint))
 if camera:local['RF_REPLAY_INSPECTION_CAMERA']=camera
 with (folder/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(inputs),str(folder/(name+'.ppm'))],env=local,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,(name,result.returncode)
 data=checkpoint.read_bytes()
 if baseline is None:baseline=data
 assert data==baseline,('inspection changed gameplay save',name)
 with Image.open(folder/(name+'.ppm')) as im:im.save(folder/(name+'.png'))
 report[name]=dict(camera=camera,checkpoint_bytes=len(data),matches_baseline=True)
report.update(result='PASS',scope='Checkpoint-invariant render camera; visual inspection is separate, not playable elevated player placement')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
