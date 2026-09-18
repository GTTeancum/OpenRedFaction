"""Guarded side-post rocket, rejected trim contact, saved continuation comparison."""
import argparse,json,math,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=int,choices=(75,79,99,103),default=79);args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts'/('side-post'+str(args.source));folder.mkdir(exist_ok=True)
target_x=-8.449 if args.source in (75,79) else 9.449
spawn_x=-2.75 if args.source in (75,79) else 3.75
target_z=-2.5 if args.source in (75,99) else 2.5
yaw=-math.pi/2 if args.source in (75,79) else math.pi/2
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE=str(args.source),RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='235')
def values(lines,label,kind=int):return list(map(kind,next(l for l in reversed(lines) if l.startswith(label+' ')).split()[1:]))
def check_impact(lines,y):
    impact=values(lines,'ROCKET_IMPACT',float)[2:]
    surface_x=target_x+(-.051 if args.source in (75,79) else .051)
    assert abs(impact[0]-surface_x)<.002 and abs(impact[1]-y)<.002 and abs(impact[2]-target_z)<.002,impact
    return impact

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
assert abs(body[0]-spawn_x)<.001 and abs(body[2]-3*target_z)<.001,body
initial_yaw=yaw
yaw=math.atan2(target_x-eye[0],target_z-eye[2])
yaw_commands,_=pitch_commands(initial_yaw,yaw,60)
# Lower post contact must preserve the protected trim: no committed terrain edit.
blocked=bytearray(header+bytes(400*48))
for f in (10,20,30,40):struct.pack_into('<I',blocked,8+48*f+40,1)
commands,_=pitch_commands(0,pitch_for(eye,[target_x,-1,target_z]))
for i,v in enumerate(commands):struct.pack_into('<f',blocked,8+48*(190+i)+12,v)
for i,v in enumerate(yaw_commands):struct.pack_into('<f',blocked,8+48*(160+i)+16,v)
struct.pack_into('<I',blocked,8+48*240+32,1)
rejected,_=run('guard-rejected',blocked)
assert values(rejected,'GEOMOD')[1]==0
rejected_impact=check_impact(rejected,-1)
assert any(l.startswith('GEOMOD_PUBLICATION_REJECT geometry -3') for l in rejected)
first=pitch_for(eye,[target_x,.8,target_z]);commands,_=pitch_commands(0,first)
data=bytearray(header+bytes(600*48))
for f in (10,20,30,40):struct.pack_into('<I',data,8+48*f+40,1)
for i,v in enumerate(commands):struct.pack_into('<f',data,8+48*(190+i)+12,v)
for i,v in enumerate(yaw_commands):struct.pack_into('<f',data,8+48*(160+i)+16,v)
struct.pack_into('<I',data,8+48*240+32,1)
shot,save=run('shot',data);assert values(shot,'AUTHORED_SOURCE_CUTS')[:2]==[args.source,1]
accepted_impact=check_impact(shot,.8)
resume=bytearray(header+bytes(301*48))
resumed,resumed_bytes=run('resume',resume,'shot')
control,control_bytes=run('control',data+resume[8+48:])
assert values(resumed,'GEOMOD')[1]==1,values(resumed,'GEOMOD')
assert resumed_bytes==control_bytes,'Saved post continuation differs from uninterrupted play'
report=dict(source=args.source,body=body,accepted_impact=accepted_impact,rejected_impact=rejected_impact,protected_trim_rejection=True,first_save_bytes=len(save),continued_save_bytes=len(resumed_bytes),continuation_equal=True,
            publication=values(resumed,'TERRAIN_PUBLICATION'),pieces=values(resumed,'DETACHED_MOTION'))
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
