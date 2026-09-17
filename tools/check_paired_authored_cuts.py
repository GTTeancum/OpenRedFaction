"""Two actual rockets aimed at different retained sources, using process-local input."""
import json, math, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_for, pitch_commands
ROOT = Path(__file__).resolve().parents[1]
folder = ROOT/'artifacts/paired-two-shot'
folder.mkdir(parents=True, exist_ok=True)
data = bytearray((ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes())
assert data[:8] == b'RFI6'+struct.pack('<I',48) and len(data)==8+550*48
data += bytes(300*48)
eye = json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
first, second = [-4.699,.25,2.5], [-4.699,.25,-2.5]
yaw0, yaw1 = -math.pi/2, math.atan2(second[0]-eye[0], second[2]-eye[2])
for offset,start,end in [(12,pitch_for(eye,first),pitch_for(eye,second)),(16,yaw0,yaw1)]:
    commands,_ = pitch_commands(start,end,60)
    for i,command in enumerate(commands):struct.pack_into('<f',data,8+48*(350+i)+offset,command)
struct.pack_into('<I',data,8+48*440+32,1)
commands,_ = pitch_commands(yaw1,(yaw0+yaw1)/2,60)
for i,command in enumerate(commands):struct.pack_into('<f',data,8+48*(650+i)+16,command)
recording = folder/'inputs.bin'
recording.write_bytes(data)
env = {k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',
           RF_REPLAY_AUTHORED_SOURCES='2',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='240')
with (folder/'run.log').open('wb') as log:
    result = subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),
                             str(recording),str(folder/'final.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode==0,'Inspect paired-two-shot/run.log'
lines = (folder/'run.log').read_text().splitlines()
def values(label):return list(map(int,next(l for l in lines if l.startswith(label+' ')).split()[1:]))
impacts = [list(map(float,l.split()[1:])) for l in lines if l.startswith('ROCKET_IMPACT ')]
assert len(impacts)==2 and abs(impacts[0][-1]-2.5)<.02 and abs(impacts[1][-1]+2.5)<.02,impacts
cuts = [list(map(int,l.split()[1:])) for l in lines if l.startswith('AUTHORED_SOURCE_CUTS ')]
assert cuts==[[94,1,93,0,0,0,0,0],[94,1,93,1,0,0,0,0]],cuts
rockets,geomod,pieces,motion = map(values,('ROCKETS','GEOMOD','DETACHED_PIECES','DETACHED_MOTION'))
assert rockets[:2]==[2,2] and rockets[4:6]==[2,0],rockets
assert geomod[1:3]==[2,2] and geomod[4]<=13*1024*1024 and not geomod[5],geomod
assert pieces[1:3]==[2,2] and not pieces[5],pieces
assert motion[0]==2 and motion[3]==2 and not motion[6],motion
report=dict(result='PASS',eye=eye,impacts=impacts,source_cuts=cuts,rockets=rockets,geomod=geomod,pieces=pieces,motion=motion,
            scope='Two separate live rocket cuts, one per owner; both fragments remain alive and settled. No simultaneous blast, save/reset or audio-quality acceptance.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
