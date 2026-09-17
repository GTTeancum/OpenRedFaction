"""Ordinary jump onto natural rubble, exact save continuation and missing-support rejection."""
import argparse,json,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_commands,pitch_for
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--paired',action='store_true',help='Keep both authored sources and use collection checkpoints')
parser.add_argument('--second-support',action='store_true',help='Walk to source93 and test support from collection slot1')
parser.add_argument('--both-destroyed',action='store_true',help='Destroy both posts before standing on source93 rubble')
args=parser.parse_args()
if args.both_destroyed:args.second_support=True
if args.second_support:args.paired=True
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/('artifacts/paired-both-support' if args.both_destroyed else 'artifacts/paired-second-support' if args.second_support else 'artifacts/paired-rubble-standing' if args.paired else 'artifacts/geomod-postedit-re/intermediate-rubble-standing')
folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/intermediate-search/0.bin').read_bytes()
data=bytearray(source[:8+350*48]+bytes(450*48))
for frame in range(360,450):struct.pack_into('<f',data,8+frame*48+8,.8)
struct.pack_into('<I',data,8+415*48+24,1)
saved_frame=600
if args.second_support:
 prefix=bytearray(180*48)
 for frame in range(30,90):struct.pack_into('<f',prefix,frame*48,-5/6)
 if args.both_destroyed:
  second_shot=bytearray(160*48)
  commands,_=pitch_commands(-.050287704,pitch_for([4.450001,.3840414,-2.5],[-4.699,-.3,-2.5]))
  for frame,value in enumerate(commands):struct.pack_into('<f',second_shot,frame*48+12,value)
  struct.pack_into('<I',second_shot,60*48+32,1)
  boundary=8+350*48
  data=data[:boundary]+prefix+second_shot+data[boundary:]
  saved_frame+=340
 else:
  data=data[:8]+prefix+data[8:]
  saved_frame+=180
recordings={'saved':data[:8+saved_frame*48],'continued':data[:8]+data[8+(saved_frame-1)*48:],'control':data}
retreat=bytearray(recordings['continued'])
for frame in range(20,100):struct.pack_into('<f',retreat,8+frame*48+8,-.8)
recordings['retreat']=retreat
recordings['retreat-control']=recordings['saved'][:-48]+retreat[8:]
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
if args.paired:env['RF_REPLAY_AUTHORED_SOURCES']='2'
def support_bank(checkpoint):
 if not args.paired:
  bank=checkpoint.rfind(b'RFPB');assert bank>=0
  return bank,struct.unpack_from('<I',checkpoint,bank+8)[0]
 assert struct.unpack_from('<I',checkpoint,16)[0]==3
 directory=576+416
 assert checkpoint[directory:directory+4]==b'RFAS'
 assert struct.unpack_from('<I',checkpoint,directory+12)[0]==2
 cursor=directory+16+2*48
 target=93 if args.second_support else 94
 for slot in range(2):
  uid,core_bytes,piece_bytes=struct.unpack_from('<III',checkpoint,directory+16+48*slot)
  assert uid==[94,93][slot]
  bank=cursor+core_bytes
  assert checkpoint[bank:bank+4]==b'RFPB'
  if uid==target:
   assert piece_bytes==344 and struct.unpack_from('<I',checkpoint,bank+12)[0]==1
   result=(bank,piece_bytes)
  elif args.both_destroyed:assert piece_bytes==344 and struct.unpack_from('<I',checkpoint,bank+12)[0]==1
  else:assert piece_bytes==16 and struct.unpack_from('<I',checkpoint,bank+12)[0]==0
  cursor=bank+piece_bytes
 return result
report={}
for name,recording in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(recording);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name in ('continued','retreat'):local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label,kind=int):return list(map(kind,next(x for x in lines if x.startswith(label+' ')).split()[1:]))
 position=values('CAMPAIGN_FINAL_POSITION',float);pieces=values('DETACHED_PIECES');contacts=values('DETACHED_PLAYER')
 expected_pieces=2 if args.both_destroyed else 1
 assert pieces[:3]==[1,expected_pieces,expected_pieces] and pieces[5]==0,(name,pieces)
 assert contacts[0]>0 and contacts[6]==0,(name,contacts)
 assert contacts[1]>0 and contacts[2]==0xffffffff,(name,contacts)
 if name.startswith('retreat'):assert position[0]>-4 and position[1]<0,(name,position)
 else:assert -5.6<position[0]<-4.4 and position[1]>.2,(name,position)
 checkpoint=path.with_suffix('.rfcp').read_bytes();bank,_=support_bank(checkpoint)
 radius=struct.unpack_from('<f',checkpoint,bank+16+256)[0]
 assert .5<radius<=1,(name,radius)
 report[name]=dict(position=position,pieces=pieces,contacts=contacts)
assert (folder/'continued.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'continuation differs'
assert (folder/'retreat.rfcp').read_bytes()==(folder/'retreat-control.rfcp').read_bytes(),'walk-away continuation differs'
# Counterfactual: same saved player pose, but retire its sole supporting chunk.
bad=bytearray((folder/'saved.rfcp').read_bytes());bank,size=support_bank(bad)
struct.pack_into('<fI',bad,bank+size-8,-1,0x200002)
(folder/'missing-support.rfcp').write_bytes(bad)
with (folder/'missing-support.log').open('wb') as log:
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(folder/'continued.bin'),str(folder/'missing-support.ppm')],cwd=ROOT,env=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(folder/'missing-support.rfcp')),stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode!=0 and 'GEOMOD_CHECKPOINT_ERROR load' in (folder/'missing-support.log').read_text(),'retired support accepted'
report['both_destroyed']=args.both_destroyed
report['paired_sources']=args.paired
report['support_source']=93 if args.second_support else 94
report['scope']='Ordinary jump lands on natural intermediate rubble; exact saved standing/walk-away continuation and retired-support rejection. Native and visual acceptance remain separate.'
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
