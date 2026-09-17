"""Ordinary walking into a settled chunk and retreat; no host input."""
import json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/detached-player';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes();report={}
for name,count in [('pass',650),('retreat',820)]:
 data=bytearray(source[:8+350*48]+bytes((count-350)*48))
 for frame in range(350,count):
  forward=.8 if 360<=frame<600 else -.8 if name=='retreat' and 650<=frame<770 else 0
  struct.pack_into('<f',data,8+48*frame+8,forward)
 recording=folder/(name+'.bin');recording.write_bytes(data)
 env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
 env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1')
 with (folder/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(recording),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=(folder/(name+'.log')).read_text().splitlines()
 def values(label,kind=int):return list(map(kind,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 hits=values('DETACHED_PLAYER');position=values('CAMPAIGN_FINAL_POSITION',float)
 assert hits[1]>0 and hits[6]==0,(name,hits)
 report[name]=dict(contacts=hits,position=position)
assert report['pass']['position'][0]<-6 and report['pass']['position'][2]>3.2,report
assert report['retreat']['position'][0]>report['pass']['position'][0]+2.5,report
report.update(result='PASS',scope='Actual player contact, sliding around an angled chunk and retreat; no pushing, crush damage or moving-platform claim')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
