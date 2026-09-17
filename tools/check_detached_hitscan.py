"""Real rocket separation followed by pistol hit and above-fragment miss control."""
import json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/detached-hitscan';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye'];report={}
for name,height in [('hit',-.25),('miss',1.5)]:
 data=bytearray(source);commands,_=pitch_commands(pitch_for(eye,[-4.699,.25,2.5]),pitch_for(eye,[-4.699,height,2.5]))
 for frame in range(550):
  if 350<=frame<380:struct.pack_into('<f',data,8+48*frame+12,commands[frame-350])
  if frame==390:struct.pack_into('<I',data,8+48*frame+40,1)
  if frame==420:struct.pack_into('<I',data,8+48*frame+32,1)
 recording=folder/(name+'.bin');recording.write_bytes(data)
 env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
 env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(folder/(name+'.rfcp')))
 (folder/(name+'.rfcp')).unlink(missing_ok=True)
 with (folder/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(recording),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=(folder/(name+'.log')).read_text().splitlines()
 def values(label):return list(map(int,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 hits=values('DETACHED_HITSCAN');selection=values('WEAPON_SELECTION');combat=values('COMBAT');rockets=values('ROCKETS')
 assert hits[0]==1 and hits[1]==int(name=='hit') and hits[6]==0,(name,hits)
 assert selection[0]==0 and rockets[0]==1 and rockets[1]==1 and rockets[4]==1,(selection,rockets)
 assert combat[0]==2 and combat[1]==0,combat
 pieces=values('DETACHED_PIECES');assert pieces[:4]==[1,1,1,12] and pieces[5]==0,pieces
 checkpoint=(folder/(name+'.rfcp')).read_bytes();piece_bytes=struct.unpack_from('<I',checkpoint,588)[0]
 trailer=checkpoint[-piece_bytes:];assert trailer[:4]==b'RFPB' and struct.unpack_from('<3I',trailer,4)==(2,344,1)
 health,flags=struct.unpack_from('<fI',trailer,336)
 # The successful extraction notification marks transform/wake flags on both cases.
 assert health>0 and flags==(0x6000000 | (0x200000 if name=='hit' else 0)),(health,flags)
 report[name]=dict(hitscan=hits,selection=selection,combat=combat,rockets=rockets,health=health,flags=flags,pieces=pieces)
assert report['hit']['health']==report['miss']['health'], 'Below-threshold pistol damage must preserve health'
report.update(result='PASS',scope='Actual pistol hit reaches kind3 damage without health loss; above-fragment miss leaves flags unchanged; no decals, NPC cover or Xbox claim')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
