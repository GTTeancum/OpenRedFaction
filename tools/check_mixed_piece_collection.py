"""Ordinary rocket retires one member of a live three-piece beam batch."""
import json,math,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/mixed-piece-collection';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
initial=ROOT/'artifacts/xemu/render-20260917-185838/xbox-checkpoint.rfds'
def pieces(data):
 offset=data.index(b'RFPB');version,size,count=struct.unpack_from('<III',data,offset+4)
 assert version==2 and size==16+count*328
 return [dict(position=struct.unpack_from('<3f',data,offset+16+i*328+100),health=struct.unpack_from('<f',data,offset+16+i*328+320)[0],flags=struct.unpack_from('<I',data,offset+16+i*328+324)[0]) for i in range(count)]
before=pieces(initial.read_bytes());assert len(before)==3 and all(not p['flags']&2 for p in before)
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye'];target=before[0]['position']
shot=bytearray(b'RFI6'+struct.pack('<I',48)+bytes(351*48))
for offset,start,end in [(12,pitch_for(eye,[-4.699,2.25,2.5]),pitch_for(eye,target)),(16,-math.pi/2,math.atan2(target[0]-eye[0],target[2]-eye[2]))]:
 commands,_=pitch_commands(start,end,60)
 for i,value in enumerate(commands):struct.pack_into('<f',shot,8+(10+i)*48+offset,value)
struct.pack_into('<I',shot,8+100*48+32,1)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='95',RF_REPLAY_AUTHORED_SOURCES='2',RF_REPLAY_PLAYER_CHECKPOINT='1')
def run(name,data,load):
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(load),RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],env=local,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,(name,result.returncode)
 return path.with_suffix('.rfcp').read_bytes()
saved=run('shot',shot,initial);after=pieces(saved)
print(json.dumps(dict(before=before,after=after),indent=2),flush=True)
assert sum(bool(p['flags']&2) for p in after)==1 and len(after)==3,'Expected one retired member, two surviving members'
assert after[0]['flags']&2 and after[1:]==before[1:],'Unexpected victim or changed surviving rubble'
lines=(folder/'shot.log').read_text(encoding='utf-8').splitlines()
def values(label):return list(map(int,[line for line in lines if line.startswith(label+' ')][-1].split()[1:]))
retained=values('DETACHED_PIECES');rocket=values('DETACHED_ROCKET')
assert retained[:3]==[1,1,2] and 0<retained[4]<32768 and retained[5]==0,retained
assert rocket[1]==1 and rocket[6]==0,rocket

resume=b'RFI6'+struct.pack('<I',48)+bytes(121*48)
continued=run('resume',resume,folder/'shot.rfcp')
control=run('control',shot+bytes(120*48),initial)
assert continued==control,'Mixed collection checkpoint continuation differs'
report=dict(result='PASS',retained=retained,rocket=rocket,before=before,after=after,bytes=len(saved),scope='Real rocket contact and mixed-batch saved continuation on PC; Xbox acceptance separate')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
