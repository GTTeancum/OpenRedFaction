"""Process-local developer wall replay; installed geometry stays unchanged.
Source66 opts into a radius2 hardness65 patch on an otherwise indestructible wall.
No checkpoint support is claimed by this test.
"""
import json, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_for, pitch_commands
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/cavity-live'
folder.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',
           RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='66',
           RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='240')
def run(name,data):
    (folder/(name+'.bin')).write_bytes(data)
    with (folder/(name+'.log')).open('w') as log:
        result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',
            str(ROOT/'Installed_Game'),str(folder/(name+'.bin')),str(folder/(name+'.ppm'))],
            cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    assert result.returncode==0,(name,result.returncode)
    return (folder/(name+'.log')).read_text().splitlines()
def values(lines,label,kind=int):
    return list(map(kind,next(l for l in reversed(lines) if l.startswith(label+' ')).split()[1:]))
header=b'RFI6'+struct.pack('<I',48)
idle=run('idle',header+bytes(48*180))
body=values(idle,'CAMPAIGN_FINAL_POSITION',float)
assert abs(body[0]+24)<.001 and abs(body[1]-2.879973)<.001 and abs(body[2]-8)<.001,body
offsets=struct.unpack('<6f',struct.pack('<6I',*values(idle,'PLAYER_CLASS_EYE')))
eye=body[:];eye[1]+=offsets[1]
commands,_=pitch_commands(0,pitch_for(eye,[-32.949,4,8]))
data=bytearray(header+bytes(48*400))
for frame in (10,20,30,40):struct.pack_into('<I',data,8+48*frame+40,1)
for i,value in enumerate(commands):struct.pack_into('<f',data,8+48*(190+i)+12,value)
aim=run('aim',data)
struct.pack_into('<I',data,8+48*240+32,1)
shot=run('shot',data)
assert values(shot,'AUTHORED_SOURCE_CUTS')[:2]==[66,1]
assert values(shot,'GEOMOD')[1:3]==[1,1]
assert values(shot,'TERRAIN_PUBLICATION')[:4]==[185,885,1,1]
assert values(shot,'PLAYER_LIFE')[0]==0
assert values(shot,'CAMPAIGN_FINAL_POSITION',float)==body
assert not any('REJECT' in line for line in shot)
impact=values(shot,'ROCKET_IMPACT',float)
assert impact==[267,3,-33,4,8],impact
report=dict(body=body,impact=impact,publication=values(shot,'TERRAIN_PUBLICATION'),
            scope='one ordinary rocket, developer-only hardness patch, no wall save/reload',
            visual_review='Inspect aim.ppm and shot.ppm separately; state assertions alone do not verify appearance.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
