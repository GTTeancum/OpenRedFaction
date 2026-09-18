"""Live selected-beam rocket cut and exact player/destruction checkpoint continuation."""
import argparse, json, math, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_commands, pitch_for
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--save-frames',type=int,default=600,help='First-blast save frame count,250..600')
parser.add_argument('--moving-first-save',action='store_true',help='Require active debris in the first saved state')
parser.add_argument('--source',type=int,choices=(95,98),default=95,help='Authored beam owner')
parser.add_argument('--next-shot',action='store_true',help='Shoot the other end of the beam after reload')
parser.add_argument('--connected',action='store_true',help='Include the attached positive-Z post and require the first blast to cut both owners')
parser.add_argument('--both-posts',action='store_true',help='Include both attached posts with the selected beam')
parser.add_argument('--settle',action='store_true',help='Continue debris for 600 updates after the saved blast and compare uninterrupted state')
parser.add_argument('--airborne',action='store_true',help='Save the second blast earlier and require moving debris before settling')
parser.add_argument('--trace',action='store_true',help='Retain detailed process-local gameplay diagnostics')
args=parser.parse_args()
if not 250<=args.save_frames<=600:parser.error('--save-frames must be250..600')
if args.moving_first_save:args.settle=True
if args.airborne:args.next_shot=True;args.settle=True
if args.both_posts:args.connected=True
prefix='triple-beam' if args.both_posts else 'connected-beam' if args.connected else 'beam'
if args.save_frames!=600:prefix+='-early'+str(args.save_frames)
folder=ROOT/'artifacts'/((('east-' if args.source==98 else '')+prefix)+('-airborne' if args.airborne else '-next-shot' if args.next_shot else '-live'))
folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48) and len(source)>=8+350*48
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
initial_yaw=-math.pi/2
target_x=-4.699
if args.source==98:
 eye[0]=1-eye[0];target_x=1-target_x;initial_yaw=math.pi/2
save=bytearray((source[:8+350*48]+bytes(250*48))[:8+args.save_frames*48])
commands,first_pitch=pitch_commands(0,pitch_for(eye,[target_x,2.25,2.5]))
for i,value in enumerate(commands):struct.pack_into('<f',save,8+48*(190+i)+12,value)
resume=bytearray(source[:8]+bytes((181 if args.airborne else 301 if args.next_shot else 121)*48))
if args.next_shot:
 target=[target_x,2.25,-2.5]
 for offset,start,end in [(12,first_pitch,pitch_for(eye,target)),(16,initial_yaw,math.atan2(target[0]-eye[0],target[2]-eye[2]))]:
  commands,_=pitch_commands(start,end,60)
  for i,value in enumerate(commands):struct.pack_into('<f',resume,8+48*(10+i)+offset,value)
 struct.pack_into('<I',resume,8+100*48+32,1)
inputs={'save':save,'resume':resume,'control':save+resume[8+48:]}
if args.settle:
 inputs['settle']=source[:8]+bytes(601*48)
 inputs['settle-control']=inputs['control']+bytes(600*48)
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_AUTHORED_SOURCE=str(args.source))
if args.connected:env['RF_REPLAY_AUTHORED_SOURCES']='3' if args.both_posts else '2'
if args.trace:env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='240')
report={}
for name,data in inputs.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='resume':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'save.rfcp')
 if name=='settle':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'resume.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0 and path.with_suffix('.rfcp').exists(),(name,result.returncode)
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label):return list(map(int,[line for line in lines if line.startswith(label+' ')][-1].split()[1:]))
 if args.both_posts:assert values('AUTHORED_COLLECTION')[:4]==[3,args.source,args.source-1,args.source-2]
 elif args.connected:assert values('AUTHORED_COLLECTION')[:3]==[2,args.source,args.source-1]
 else:assert values('AUTHORED_SOURCE')[0]==args.source
 expected=2 if args.next_shot and name!='save' else 1
 geomod=values('GEOMOD');assert geomod[1]==expected+int(args.connected)+int(args.both_posts and expected==2),(name,geomod)
 if name!='resume':
  source_cuts=values('AUTHORED_SOURCE_CUTS')
  assert source_cuts[:2]==[args.source,expected]
  if args.connected:assert source_cuts[2:4]==[args.source-1,1]
  if args.both_posts:assert source_cuts[4:6]==[args.source-2,expected-1]
 if args.moving_first_save and name=='save':
  motion=values('DETACHED_MOTION')
  assert motion[0]>motion[3] and motion[6]==0,('first saved rubble is not moving',motion)
 checkpoint=path.with_suffix('.rfcp').read_bytes();assert checkpoint[:4]==b'RFCP'
 offset=checkpoint.find(b'RFDS');assert offset>=0
 payload=checkpoint[offset:];assert struct.unpack_from('<I',payload,312)[0]==2
 maps,core=struct.unpack_from('<II',payload,248);admissions=struct.unpack_from('<I',payload,240)[0]
 tokens=[struct.unpack_from('<I',payload,416+core+admissions*48+i*88+40)[0] for i in range(maps)]
 if args.source==98 and args.both_posts:
  # The opposite roof has different compiled partitions, so chart counts need
  # not match beam95; connected charts must still resolve to authored wood.
  assert tokens and all(token==0 for token in tokens),(name,tokens)
 elif args.both_posts:assert tokens==[0]*(7 if expected==2 else 3),(name,tokens)
 elif args.connected:assert tokens==[0,0,0]+([0,0,4] if expected==2 else []),(name,tokens)
 else:assert tokens==[0,0,4]*expected,(name,tokens)
 pieces=values('DETACHED_PIECES')[2]
 assert pieces>=expected if args.source==98 else pieces==expected
 report[name]=dict(bytes=len(checkpoint),geomod=geomod,material_tokens=tokens,pieces=pieces,motion=values('DETACHED_MOTION'))
 print(name,report[name],flush=True)
assert (folder/'resume.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'beam continuation differs'
if args.settle:
 settled=(folder/'settle.rfcp').read_bytes()
 assert settled==(folder/'settle-control.rfcp').read_bytes(),'settled continuation differs'
 lines=(folder/'settle.log').read_text(encoding='utf-8').splitlines()
 motion=list(map(int,[line for line in lines if line.startswith('DETACHED_MOTION ')][-1].split()[1:]))
 expected=report['resume']['pieces']
 assert motion[0]==expected and motion[3]==expected and motion[6]==0,('fragments did not settle',motion)
 # Validate all serialized piece banks, not only the first source's bank.
 positions=[];offset=0
 while True:
  offset=settled.find(b'RFPB',offset)
  if offset<0:break
  version,size,count=struct.unpack_from('<III',settled,offset+4)
  assert version==2 and size==16+328*count
  for i in range(count):
   position=struct.unpack_from('<3f',settled,offset+16+i*328+12+88)
   assert all(math.isfinite(v) for v in position) and position[1]>=-1.5,('fragment below floor',position)
   positions.append(position)
  offset+=size
 assert len(positions)==expected
 before_lines=(folder/'resume.log').read_text(encoding='utf-8').splitlines()
 before=list(map(int,[line for line in before_lines if line.startswith('DETACHED_MOTION ')][-1].split()[1:]))
 if args.airborne:assert before[0]==expected and before[3]<expected and before[6]==0,('expected moving debris at save',before)
 report['settling']=dict(before=before,motion=motion,positions=positions,updates=600,scope='Moving debris save through landing' if args.airborne else 'Settled retention when bodies already sleep at reload; does not require an airborne save')
report.update(result='PASS',source=args.source,save_frames=args.save_frames,moving_first_save=args.moving_first_save,airborne=args.airborne,next_shot=args.next_shot,connected=args.connected,both_posts=args.both_posts,scope='Actual beam rocket, optional second cut after reload, retained wood charts/fragments and exact PC player/destruction continuation; visual and Xbox acceptance separate')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
