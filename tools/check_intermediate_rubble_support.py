"""Jump against natural rubble and save after landing beside it.
Originally exposed ignored collision and no-boundary-ray save bugs. This route
does not land on top of the chunk; saved rubble standing remains unverified.
The save is after rendered frame599, before consuming input599; replay that
input as the first resumed frame so both paths execute the same200 steps.
No host input.
"""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/intermediate-rubble-support';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/intermediate-search/0.bin').read_bytes()
data=bytearray(source[:8+350*48]+bytes(450*48))
for frame in range(360,471):struct.pack_into('<f',data,8+frame*48+8,.8)
struct.pack_into('<I',data,8+470*48+24,1)
recordings={'saved':data[:8+600*48],'continued':data[:8]+data[8+599*48:],'control':data}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,recording in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='continued':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label,kind=int):return list(map(kind,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 position=values('CAMPAIGN_FINAL_POSITION',float);pieces=values('DETACHED_PIECES');contacts=values('DETACHED_PLAYER')
 assert pieces[:3]==[1,1,1] and pieces[5]==0,(name,pieces)
 assert contacts[0]>0 and contacts[6]==0,(name,contacts)
 if name=='continued':assert contacts[1]==0,(name,contacts)
 else:assert contacts[1]>0 and contacts[2]==0xffffffff,(name,contacts)
 assert position[0]>-4 and position[1]<0,(name,position)
 checkpoint=path.with_suffix('.rfcp').read_bytes();bank=checkpoint.rfind(b'RFPB')
 assert bank>=0
 radius=struct.unpack_from('<f',checkpoint,bank+16+256)[0]
 assert .5<radius<=1,(name,radius)
 report[name]=dict(position=position,pieces=pieces,contacts=contacts)
assert (folder/'continued.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'continuation differs'
report['scope']='Jump contact with natural intermediate rubble, landing beside it and exact save continuation; saved standing on rubble, native and visual acceptance remain separate.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
