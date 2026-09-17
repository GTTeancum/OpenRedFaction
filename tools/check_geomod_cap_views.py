"""Inspect real saved beam joint cuts from elevated render-only views."""
import json,os,struct,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-cap-views';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
inputs=folder/'neutral.bin';inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(121*48))
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='95',RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(ROOT/'artifacts/xemu/render-20260917-191001/xbox-checkpoint.rfds'))
views={'baseline':None,'near-cap':'-3,2.25,0,-5,1.5,2.5','far-cap':'-3,2.25,0,-5,1.5,-2.5','far-reverse':'-4,2.25,-4,-5,1.5,-2.5','uncut-far':'-3,2.25,0,-5,1.5,-2.5'}
report={};baseline=None
for name,camera in views.items():
 checkpoint=folder/(name+'.rfcp');checkpoint.unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(checkpoint),RF_REPLAY_DEPTH_OUT=str(folder/(name+'.depth')))
 if camera:local['RF_REPLAY_INSPECTION_CAMERA']=camera
 if name=='uncut-far':local.pop('RF_REPLAY_GEOMOD_CHECKPOINT_IN')
 with (folder/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(inputs),str(folder/(name+'.ppm'))],env=local,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,(name,result.returncode)
 data=checkpoint.read_bytes()
 if baseline is None:baseline=data
 if name!='uncut-far':assert data==baseline,('inspection changed gameplay save',name)
 with Image.open(folder/(name+'.ppm')) as im:im.save(folder/(name+'.png'))
 report[name]=dict(camera=camera,checkpoint_bytes=len(data),matches_baseline=data==baseline)
# Fixed unobstructed floor region, selected from inspected elevated captures.
# The neighboring uncut post occludes pixels left of x354, so those are excluded.
region=(360,268,410,281)
with Image.open(folder/'far-cap.png') as cut,Image.open(folder/'uncut-far.png') as original:
 assert cut.crop(region).tobytes()==original.crop(region).tobytes(),'floor detail color changed'
depth=(folder/'far-cap.depth').read_bytes();uncut=(folder/'uncut-far.depth').read_bytes()
assert depth[:12]==uncut[:12]==b'RFD1'+struct.pack('<II',640,480)
for y in range(region[1],region[3]):
 start=12+(y*640+region[0])*4;end=12+(y*640+region[2])*4
 assert depth[start:end]==uncut[start:end],'floor detail depth changed'
line=next(l for l in (folder/'far-cap.log').read_text(encoding='utf-8').splitlines() if l.startswith('DEPTH_CAMERA '))
pose=struct.unpack('<12f',struct.pack('<12I',*map(int,line.split()[2:])))
x,y=375,275;zbuffer=struct.unpack_from('<f',depth,12+(y*640+x)*4)[0]
z=.1/(1-zbuffer/16777215/(1000/999.9))
view=((x+.5-320)*z/320,(240-y-.5)*z/320,z)
world=[pose[k]+sum(view[j]*pose[3+j*3+k] for j in range(3)) for k in range(3)]
assert abs(world[1]+2)<.002,('patch is not on expected floor',world)
report['floor_patch']=dict(region=region,color_and_depth_identical=True,pixel=[x,y],world=world,scope='Pre-existing floor detail, not a destruction-created floating polygon')
report.update(result='PASS' ,scope='Checkpoint-invariant render camera; visual inspection is separate, not playable elevated player placement')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
