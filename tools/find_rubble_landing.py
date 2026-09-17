"""Bounded ordinary-control search for standing on naturally extracted rubble."""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/rubble-landing-search';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/intermediate-search/0.bin').read_bytes()
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
rows=[]
for jump,stop in [(415,450),(420,450),(425,450)]:
 path=folder/f'{jump}-{stop}';data=bytearray(source[:8+350*48]+bytes(450*48))
 for frame in range(360,stop):struct.pack_into('<f',data,8+frame*48+8,.8)
 struct.pack_into('<I',data,8+jump*48+24,1)
 path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp'))),stdout=log,stderr=subprocess.STDOUT,timeout=180)
 row=dict(jump=jump,stop=stop,exit=result.returncode)
 for line in path.with_suffix('.log').read_text().splitlines():
  if line.startswith(('CAMPAIGN_FINAL_POSITION ','DETACHED_PLAYER ','PLAYER_CHECKPOINT_REJECT ')):row[line.split()[0]]=line.split()[1:]
 rows.append(row);print(row,flush=True)
(folder/'report.json').write_text(json.dumps(rows,indent=2)+'\n')
