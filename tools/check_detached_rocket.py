"""Two real process-local rockets: detach post, then hit its settled chunk."""
import json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/geomod-postedit-re/detached-rocket';folder.mkdir(parents=True,exist_ok=True)
data=bytearray((ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes())
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
commands,_=pitch_commands(pitch_for(eye,[-4.699,.25,2.5]),pitch_for(eye,[-4.699,-.25,2.5]))
for frame in range(550):
 if 350<=frame<380:struct.pack_into('<f',data,8+48*frame+12,commands[frame-350])
 if frame==420:struct.pack_into('<I',data,8+48*frame+32,1)
recording=folder/'inputs.bin';recording.write_bytes(data)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(folder/'saved.rfcp'))
(folder/'saved.rfcp').unlink(missing_ok=True)
with (folder/'run.log').open('wb') as log:
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(recording),str(folder/'final.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode==0,'Inspect run.log'
lines=(folder/'run.log').read_text().splitlines()
def values(label):return list(map(int,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
hits=values('DETACHED_ROCKET');rockets=values('ROCKETS');impact=values('IMPACT_AUDIO')
assert hits[1]==1 and hits[6]==0,hits
assert rockets[0]==2 and rockets[1]==2 and rockets[4]==1,rockets
assert impact[0]==2,impact
pieces=values('DETACHED_PIECES');motion=values('DETACHED_MOTION')
assert pieces[:4]==[1,1,0,0] and pieces[5]==0,pieces
assert motion==[0]*8,motion
report=dict(result='PASS',detached_rocket=hits,rockets=rockets,impact_audio=impact,detached_pieces=pieces,detached_motion=motion,scope='Two actual rockets; second retires settled chunk from draw/motion, with only initial terrain edit; audible quality and Xbox unverified')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
