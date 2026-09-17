"""Bounded natural extraction search: vary only recorded rocket aiming."""
import json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/intermediate-search';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
rows=[]
for index,height in enumerate([-.1,0,.1]):
 path=folder/str(index);data=bytearray(source[:8+350*48]+bytes(250*48))
 commands,_=pitch_commands(0,pitch_for(eye,[-4.699,height,2.5]))
 for j,value in enumerate(commands):struct.pack_into('<f',data,8+48*(190+j)+12,value)
 path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp'))),stdout=log,stderr=subprocess.STDOUT,timeout=180)
 row=dict(case=index,height=height,exit=result.returncode,radii=[])
 if result.returncode==0:
  checkpoint=path.with_suffix('.rfcp').read_bytes();offset=checkpoint.rfind(b'RFPB');assert offset>=0
  version,size,count=struct.unpack_from('<3I',checkpoint,offset+4);assert version==2 and size==16+328*count and offset+size==len(checkpoint)
  row['radii']=[struct.unpack_from('<f',checkpoint,offset+16+328*j+256)[0] for j in range(count)]
 rows.append(row);print(row,flush=True)
(folder/'report.json').write_text(json.dumps(rows,indent=2)+'\n')
