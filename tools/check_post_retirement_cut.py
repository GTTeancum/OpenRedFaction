"""A third ordinary blast after retirement, compared with checkpoint continuation."""
import argparse,json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--height',type=float,default=1.25);args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re'/('post-retirement-cut' if args.height==1.25 else f'post-retirement-cut-{args.height:g}');folder.mkdir(parents=True,exist_ok=True)
prior=ROOT/'artifacts/geomod-postedit-re/detached-rocket';source=(prior/'inputs.bin').read_bytes();data=bytearray(source+bytes(250*48))
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
commands,_=pitch_commands(pitch_for(eye,[-4.699,-.25,2.5]),pitch_for(eye,[-4.699,args.height,2.5]))
for i,value in enumerate(commands):struct.pack_into('<f',data,8+(550+i)*48+12,value)
struct.pack_into('<I',data,8+620*48+32,1)
recordings={'control':data,'continued':data[:8]+data[8+549*48:]}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,recording in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='continued':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(prior/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name+' inspect log'
 rows={line.split()[0]:line.split()[1:] for line in path.with_suffix('.log').read_text().splitlines() if line.startswith(('ROCKETS ','DETACHED_PIECES ','DETACHED_MOTION '))}
 report[name]={k:list(map(int,v)) for k,v in rows.items()};print(name,report[name],flush=True)
 assert report[name]['ROCKETS'][4]==(2 if name=='control' else 1),report[name]
 assert report[name]['DETACHED_PIECES'][5]==0,report[name]
assert (folder/'control.rfcp').read_bytes()==(folder/'continued.rfcp').read_bytes(),'post-retirement cut continuation differs'
report.update(result='PASS',scope='New terrain cut after ordinary rocket retirement/collection; exact uninterrupted versus reloaded checkpoint. New fragment extraction is reported separately.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS')
