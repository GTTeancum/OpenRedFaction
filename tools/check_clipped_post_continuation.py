"""Actual Y0.3 post cut: strict publication, RFCP reload and continuation."""
import argparse, json, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_commands, pitch_for
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--next-shot',action='store_true',help='Fire a second rocket at the retained upper post after reload')
args=parser.parse_args()
folder=ROOT/('artifacts/clipped-post-next-shot' if args.next_shot else 'artifacts/clipped-post-continuation');folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48) and len(source)>=8+350*48
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text(encoding='utf-8'))['eye']
save=bytearray(source[:8+350*48]+bytes(250*48))
commands,first_pitch=pitch_commands(0,pitch_for(eye,[-4.699,.3,2.5]))
for i,value in enumerate(commands):struct.pack_into('<f',save,8+48*(190+i)+12,value)
# Restored frame0 publishes existing state: 121 frames provide120 updates.
resume=bytearray(source[:8]+bytes((241 if args.next_shot else 121)*48))
if args.next_shot:
 commands,_=pitch_commands(first_pitch,pitch_for(eye,[-4.699,1.5,2.5]))
 for i,value in enumerate(commands):struct.pack_into('<f',resume,8+48*(10+i)+12,value)
 struct.pack_into('<I',resume,8+60*48+32,1)
inputs={'save':save,'resume':resume,'control':save+resume[8+48:]}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,data in inputs.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='resume':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'save.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0 and path.with_suffix('.rfcp').exists(),name
 lines=path.with_suffix('.log').read_text(encoding='utf-8').splitlines()
 def values(label):return list(map(int,[line for line in lines if line.startswith(label+' ')][-1].split()[1:]))
 expected=2 if args.next_shot and name!='save' else 1
 geomod=values('GEOMOD');assert geomod[1]==expected,(name,geomod)
 if name!='resume':assert values('AUTHORED_SOURCE_CUTS')[:2]==[94,expected]
 checkpoint=path.with_suffix('.rfcp').read_bytes();bank=checkpoint.rfind(b'RFPB')
 assert bank>=0 and checkpoint[:4]==b'RFCP'
 version,size,count=struct.unpack_from('<3I',checkpoint,bank+4)
 assert version==2 and size==16+328*count and count>=1 and bank+size==len(checkpoint)
 radius=struct.unpack_from('<f',checkpoint,bank+16+256)[0]
 assert abs(radius-.434212536)<1e-6,(name,radius)
 if name!='resume':assert values('GEOMOD')[5:8]==[0,expected,expected]
 elif args.next_shot:assert values('GEOMOD')[5:8]==[0,1,1]
 report[name]=dict(bytes=len(checkpoint),radius=radius,retained_bodies=count,geomod=geomod)
assert (folder/'resume.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'partitioned publication continuation differs'
report.update(result='PASS',next_shot=args.next_shot,scope=('Actual previously rejected post cut, reload/next rocket commits a second cut and matches uninterrupted PC bytes; Xbox and visual acceptance separate' if args.next_shot else 'Actual previously rejected post cut, retained fragment and exact PC player/destruction save continuation; Xbox and visual acceptance separate'))
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
