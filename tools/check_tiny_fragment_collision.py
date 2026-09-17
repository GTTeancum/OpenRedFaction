"""Two actual rockets create a tiny fragment; check continued simulation and saves."""
import json, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_commands, pitch_for
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/tiny-fragment-collision';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/intermediate-search/0.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48)
move=bytearray(180*48)
for frame in range(30,90):struct.pack_into('<f',move,frame*48,-5/6)
shot=bytearray(410*48)
commands,_=pitch_commands(-.050287704,pitch_for([4.450001,.3840414,-2.5],[-4.699,-.5,-2.5]))
for frame,value in enumerate(commands):struct.pack_into('<f',shot,frame*48+12,value)
struct.pack_into('<I',shot,60*48+32,1)
save=source[:8+350*48]+move+shot
inputs={'save':save,'resume':source[:8]+bytes(201*48),'control':save+bytes(200*48)}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCES='2',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,data in inputs.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='resume':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'save.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0 and path.with_suffix('.rfcp').exists(),name
 lines=path.with_suffix('.log').read_text(encoding='utf-8').splitlines()
 def values(label):return list(map(int,[l for l in lines if l.startswith(label+' ')][-1].split()[1:]))
 assert values('AUTHORED_SOURCE_CUTS')==[94,1,93,1,0,0,0,0]
 motion=values('DETACHED_MOTION');assert motion[0]==2 and motion[6]==0
 checkpoint=path.with_suffix('.rfcp').read_bytes();assert struct.unpack_from('<I',checkpoint,16)[0]==3
 cursor=1104;radii=[]
 for slot in range(2):
  uid,core,pieces=struct.unpack_from('<III',checkpoint,1008+48*slot)
  assert uid==[94,93][slot] and pieces==344
  bank=cursor+core;assert checkpoint[bank:bank+4]==b'RFPB'
  radii.append(struct.unpack_from('<f',checkpoint,bank+16+256)[0]);cursor=bank+pieces
 assert radii[0]>.5 and 0<radii[1]<.05,radii
 report[name]=dict(bytes=len(checkpoint),radii=radii,motion=motion)
assert (folder/'resume.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'tiny-body continuation differs'
report.update(result='PASS',scope='Tiny fragment no longer aborts scene simulation; both-source save continuation matches PC. Tiny body is born below the floor and keeps falling; out-of-world retirement and paired standing are separate work.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
