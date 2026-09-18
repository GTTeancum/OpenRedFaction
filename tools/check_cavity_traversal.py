"""Native process-local movement into a real cavity cut; no host input."""
import json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/cavity-traversal';folder.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
           RF_REPLAY_AUTHORED_SOURCE='66',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='790')
header=b'RFI6'+struct.pack('<I',48)
def values(lines,label,kind=int):return list(map(kind,next(l for l in reversed(lines) if l.startswith(label+' ')).split()[1:]))
def run(name,data,load=None):
    path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
    local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
    if load:local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/(load+'.rfcp'))
    with path.with_suffix('.log').open('w') as log:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),
            str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,(name,result.returncode)
    lines=path.with_suffix('.log').read_text().splitlines()
    assert values(lines,'PLAYER_LIFE')[0]==0,name
    return lines,path.with_suffix('.rfcp').read_bytes()
# Measured settled walkway position, with authored miner eye offset.
eye=[-24,2.879973+.785402536,8]
commands,_=pitch_commands(0,pitch_for(eye,[-32.949,.5,8]))
data=bytearray(header+bytes(1200*48))
for frame in (10,20,30,40):struct.pack_into('<I',data,8+48*frame+40,1)
for i,value in enumerate(commands):struct.pack_into('<f',data,8+48*(190+i)+12,value)
for frame in range(800,1050):struct.pack_into('<f',data,8+48*frame+8,1)
struct.pack_into('<I',data,8+48*960+24,1)
intact,_=run('intact',data)
struct.pack_into('<I',data,8+48*240+32,1)
entered,inside_save=run('entered',data)
intact_pos=values(intact,'CAMPAIGN_FINAL_POSITION',float);inside_pos=values(entered,'CAMPAIGN_FINAL_POSITION',float)
assert intact_pos[0]>-33 and inside_pos[0]<-33.2,(intact_pos,inside_pos)
assert values(entered,'GEOMOD')[1]==1
assert not any('REJECT' in l for l in entered)
resume=bytearray(header+bytes(301*48))
for frame in range(20,200):struct.pack_into('<f',resume,8+48*frame+8,-.8)
returned,resumed_save=run('returned',resume,'entered')
control,control_save=run('control',data+resume[8+48:])
return_pos=values(returned,'CAMPAIGN_FINAL_POSITION',float)
assert return_pos[0]>-31 and -1.3<return_pos[1]<-1,return_pos
assert resumed_save==control_save,'Inside-crater reload/walk-out differs from uninterrupted play'
report=dict(intact=intact_pos,inside=inside_pos,returned=return_pos,inside_save_bytes=len(inside_save),
            continuation_equal=True,scope='one low wall crater, ordinary jump entry, standing save and walk-out')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
