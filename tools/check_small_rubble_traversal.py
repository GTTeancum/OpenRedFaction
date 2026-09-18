"""Walk through original-excluded small rubble; verify exact save continuation.

Supersedes the obsolete check_rubble_standing.py radius0.471 fixture. Actual
standing coverage lives in check_intermediate_rubble_standing.py (radius>0.5).
"""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/small-rubble-traversal';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48)
data=bytearray(source[:8+350*48]+bytes(450*48))
# Walk along the chunk centerline; no jump that could explain missing contact.
for frame in range(360,502):struct.pack_into('<f',data,8+frame*48+8,.8)
inputs={'before':data[:8+350*48],'saved':data[:8+600*48],
        'continued':data[:8]+data[8+599*48:],'control':data}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,recording in inputs.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='continued':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label,kind=int):return list(map(kind,[x for x in lines if x.startswith(label+' ')][-1].split()[1:]))
 position=values('CAMPAIGN_FINAL_POSITION',float);pieces=values('DETACHED_PIECES');contacts=values('DETACHED_PLAYER');jump=values('PLAYER_JUMP')
 assert pieces[:3]==[1,1,1] and pieces[5]==0 and contacts[0]>0 and contacts[1]==contacts[6]==0,(name,pieces,contacts)
 assert jump[0]==0,(name,jump)
 checkpoint=path.with_suffix('.rfcp').read_bytes();bank=checkpoint.rfind(b'RFPB');assert bank>=0
 radius=struct.unpack_from('<f',checkpoint,bank+16+256)[0]
 assert 0<radius<=.5,(name,radius)
 pose=values('DETACHED_POSE',float)
 assert abs(position[2]-pose[2])<radius and pose[3:]==[0,0,0],(name,position,pose)
 assert (position[0]>pose[0]+radius if name=='before' else position[0]<pose[0]-radius),(name,position,pose)
 report[name]=dict(position=position,piece_pose=pose,radius=radius,contacts=contacts,jump=jump,checkpoint_bytes=len(checkpoint))
assert (folder/'continued.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'continuation differs'
report.update(result='PASS',scope='Ordinary player walks across retained radius<=0.5 rubble with no jump or fragment contact, as original size admission requires; exact saved continuation. Larger rubble standing, moving supports, visual and native acceptance are separate.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
