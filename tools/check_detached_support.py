"""Two real process-local rockets: detach post, then remove its remaining support."""
import json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/geomod-postedit-re/detached-support';folder.mkdir(parents=True,exist_ok=True)
data=bytearray((ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes())
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
commands,_=pitch_commands(pitch_for(eye,[-4.699,.25,2.5]),pitch_for(eye,[-4.699,-.95,2.5]))
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
assert hits[1]==0 and hits[6]==0,hits
assert rockets[0]==2 and rockets[1]==2 and rockets[4]==2,rockets
assert impact[0]==2,impact
pieces=values('DETACHED_PIECES');motion=values('DETACHED_MOTION')
assert pieces[:4]==[1,1,1,12] and pieces[5]==0,pieces
assert motion[0]==1 and motion[3]==1 and motion[6]==0,motion
wake=[list(map(int,x.split()[1:])) for x in lines if x.startswith('DETACHED_WAKE ')]
assert len(wake)==1 and wake[0][1]==1,wake
pose=list(map(float,next(x for x in lines if x.startswith('DETACHED_POSE ')).split()[1:]))
prior=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.log').read_text().splitlines()
baseline=list(map(float,next(x for x in prior if x.startswith('DETACHED_POSE ')).split()[1:]))
assert baseline[1]-pose[1]>.2 and pose[3:]==[0,0,0],(baseline,pose)
assert abs(pose[0]-baseline[0])<.001 and abs(pose[2]-baseline[2])<.001,(baseline,pose)
report=dict(result='PASS',wake=wake,baseline_pose=baseline,final_pose=pose,detached_rocket=hits,rockets=rockets,impact_audio=impact,detached_pieces=pieces,detached_motion=motion,scope='Two actual rockets; second removes remaining terrain support, wakes chunk without direct hit, and chunk falls over0.2units then settles; audible quality and Xbox unverified')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
