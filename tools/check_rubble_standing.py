"""Ordinary jump onto a real extracted chunk, then checkpoint continuation."""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/rubble-standing';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
data=bytearray(source[:8+350*48]+bytes(450*48))
for frame in range(360,502):struct.pack_into('<f',data,8+frame*48+8,.8)
struct.pack_into('<I',data,8+470*48+24,1)
recordings={'saved':data[:8+600*48],'continued':data[:8]+data[8+600*48:],'control':data}
retreat=bytearray(recordings['continued'])
for frame in range(20,100):struct.pack_into('<f',retreat,8+frame*48+8,-.8)
recordings['retreat']=retreat
recordings['retreat-control']=recordings['saved']+retreat[8:]
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,recording in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name in ('continued','retreat'):local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name+' inspect log'
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label,kind=int):return list(map(kind,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 position=values('CAMPAIGN_FINAL_POSITION',float);pieces=values('DETACHED_PIECES');jump=values('PLAYER_JUMP');contacts=values('DETACHED_PLAYER')
 assert pieces[:3]==[1,1,1] and pieces[5]==0 and contacts[1]>0,(name,position,pieces,contacts)
 if name not in ('continued','retreat'):assert jump[0]==1 and jump[1]==1,jump
 if name.startswith('retreat'):assert position[0]>0 and position[1]<0,position
 else:assert position[1]>.4,position
 report[name]=dict(position=position,jump=jump,contacts=contacts)
a=(folder/'continued.rfcp').read_bytes();b=(folder/'control.rfcp').read_bytes();assert a==b,'continuation checkpoint differs'
assert (folder/'retreat.rfcp').read_bytes()==(folder/'retreat-control.rfcp').read_bytes(),'walk-away checkpoint differs'
# Retired support must not allow the saved player to float above the floor.
bad=bytearray((folder/'saved.rfcp').read_bytes());struct.pack_into('<fI',bad,len(bad)-8,-1,0x200002)
(folder/'missing-support.rfcp').write_bytes(bad)
local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(folder/'missing-support.rfcp'))
with (folder/'missing-support.log').open('wb') as log:
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'continued.bin'),str(folder/'missing-support.ppm')],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode!=0 and 'GEOMOD_CHECKPOINT_ERROR load' in (folder/'missing-support.log').read_text()
report.update(result='PASS',scope='Actual jump/standing save continuation, walking away after reload, and missing-support rejection; native/visual acceptance separate')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
