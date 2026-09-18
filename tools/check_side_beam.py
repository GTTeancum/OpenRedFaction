"""Ordinary side-beam92 rocket, saved second shot, uninterrupted comparison."""
import json,math,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/side-beam92';folder.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='92',RF_REPLAY_PLAYER_CHECKPOINT='1')
def values(lines,label,kind=int):return list(map(kind,next(l for l in reversed(lines) if l.startswith(label+' ')).split()[1:]))
def run(name,data,load=None):
    path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
    e=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
    if load:e['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/(load+'.rfcp'))
    with path.with_suffix('.log').open('w') as log:
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=e,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert r.returncode==0,(name,r.returncode)
    lines=path.with_suffix('.log').read_text().splitlines();assert values(lines,'PLAYER_LIFE')[0]==0
    return lines,path.with_suffix('.rfcp').read_bytes()
header=b'RFI6'+struct.pack('<I',48)
idle,_=run('idle',header+bytes(180*48));body=values(idle,'CAMPAIGN_FINAL_POSITION',float)
off=struct.unpack('<6f',struct.pack('<6I',*values(idle,'PLAYER_CLASS_EYE')))
eye=body[:];eye[1]+=off[1]
assert abs(body[0]+2.75)<.001 and abs(body[2])<.001,body
first=pitch_for(eye,[-8.449,1.75,0]);commands,_=pitch_commands(0,first)
data=bytearray(header+bytes(600*48))
for f in (10,20,30,40):struct.pack_into('<I',data,8+48*f+40,1)
for i,v in enumerate(commands):struct.pack_into('<f',data,8+48*(190+i)+12,v)
struct.pack_into('<I',data,8+48*240+32,1)
shot,save=run('shot',data);assert values(shot,'AUTHORED_SOURCE_CUTS')[:2]==[92,1]
resume=bytearray(header+bytes(301*48));target=[-8.449,1.75,2]
for offset,start,end in [(12,first,pitch_for(eye,target)),(16,-math.pi/2,math.atan2(target[0]-eye[0],target[2]-eye[2]))]:
    commands,_=pitch_commands(start,end,60)
    for i,v in enumerate(commands):struct.pack_into('<f',resume,8+48*(10+i)+offset,v)
struct.pack_into('<I',resume,8+48*100+32,1)
resumed,resumed_bytes=run('resume',resume,'shot')
control,control_bytes=run('control',data+resume[8+48:])
assert values(resumed,'GEOMOD')[1]==2,values(resumed,'GEOMOD')
assert resumed_bytes==control_bytes,'Saved beam continuation differs from uninterrupted play'
report=dict(source=92,body=body,first_save_bytes=len(save),continued_save_bytes=len(resumed_bytes),continuation_equal=True,
            publication=values(resumed,'TERRAIN_PUBLICATION'),pieces=values(resumed,'DETACHED_MOTION'))
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
