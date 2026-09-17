"""Ordinary jump onto natural rubble, exact save continuation and missing-support rejection."""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/intermediate-rubble-standing';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/intermediate-search/0.bin').read_bytes()
data=bytearray(source[:8+350*48]+bytes(450*48))
for frame in range(360,450):struct.pack_into('<f',data,8+frame*48+8,.8)
struct.pack_into('<I',data,8+415*48+24,1)
recordings={'saved':data[:8+600*48],'continued':data[:8]+data[8+599*48:],'control':data}
retreat=bytearray(recordings['continued'])
for frame in range(20,100):struct.pack_into('<f',retreat,8+frame*48+8,-.8)
recordings['retreat']=retreat
recordings['retreat-control']=recordings['saved'][:-48]+retreat[8:]
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,recording in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name in ('continued','retreat'):local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label,kind=int):return list(map(kind,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 position=values('CAMPAIGN_FINAL_POSITION',float);pieces=values('DETACHED_PIECES');contacts=values('DETACHED_PLAYER')
 assert pieces[:3]==[1,1,1] and pieces[5]==0,(name,pieces)
 assert contacts[0]>0 and contacts[6]==0,(name,contacts)
 assert contacts[1]>0 and contacts[2]==0xffffffff,(name,contacts)
 if name.startswith('retreat'):assert position[0]>-4 and position[1]<0,(name,position)
 else:assert -5.6<position[0]<-4.4 and position[1]>.2,(name,position)
 checkpoint=path.with_suffix('.rfcp').read_bytes();bank=checkpoint.rfind(b'RFPB')
 assert bank>=0
 radius=struct.unpack_from('<f',checkpoint,bank+16+256)[0]
 assert .5<radius<=1,(name,radius)
 report[name]=dict(position=position,pieces=pieces,contacts=contacts)
assert (folder/'continued.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'continuation differs'
assert (folder/'retreat.rfcp').read_bytes()==(folder/'retreat-control.rfcp').read_bytes(),'walk-away continuation differs'
# Counterfactual: same saved player pose, but retire its sole supporting chunk.
bad=bytearray((folder/'saved.rfcp').read_bytes());struct.pack_into('<fI',bad,len(bad)-8,-1,0x200002)
(folder/'missing-support.rfcp').write_bytes(bad)
with (folder/'missing-support.log').open('wb') as log:
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'continued.bin'),str(folder/'missing-support.ppm')],cwd=ROOT,env=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(folder/'missing-support.rfcp')),stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode!=0 and 'GEOMOD_CHECKPOINT_ERROR load' in (folder/'missing-support.log').read_text(),'retired support accepted'
report['scope']='Ordinary jump lands on natural intermediate rubble; exact saved standing/walk-away continuation and retired-support rejection. Native and visual acceptance remain separate.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
