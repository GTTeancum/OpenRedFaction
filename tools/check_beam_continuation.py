"""Live beam95 rocket cut and exact player/destruction checkpoint continuation."""
import json, os, struct, subprocess
from pathlib import Path
from replay_authored_post import pitch_commands, pitch_for
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/beam-live';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48) and len(source)>=8+350*48
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text())['eye']
save=bytearray(source[:8+350*48]+bytes(250*48))
commands,_=pitch_commands(0,pitch_for(eye,[-4.699,2.25,2.5]))
for i,value in enumerate(commands):struct.pack_into('<f',save,8+48*(190+i)+12,value)
resume=source[:8]+bytes(121*48)
inputs={'save':save,'resume':resume,'control':save+resume[8+48:]}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_AUTHORED_SOURCE='95')
report={}
for name,data in inputs.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='resume':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(folder/'save.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0 and path.with_suffix('.rfcp').exists(),(name,result.returncode)
 lines=path.with_suffix('.log').read_text().splitlines()
 def values(label):return list(map(int,[line for line in lines if line.startswith(label+' ')][-1].split()[1:]))
 assert values('AUTHORED_SOURCE')[0]==95
 geomod=values('GEOMOD');assert geomod[1]==1,(name,geomod)
 if name!='resume':assert values('AUTHORED_SOURCE_CUTS')[:2]==[95,1]
 checkpoint=path.with_suffix('.rfcp').read_bytes();assert checkpoint[:4]==b'RFCP'
 offset=checkpoint.find(b'RFDS');assert offset>=0
 payload=checkpoint[offset:];assert struct.unpack_from('<I',payload,312)[0]==2
 maps,core=struct.unpack_from('<II',payload,248);admissions=struct.unpack_from('<I',payload,240)[0]
 tokens=[struct.unpack_from('<I',payload,416+core+admissions*48+i*88+40)[0] for i in range(maps)]
 assert 4 in tokens and 0 in tokens,(name,tokens)
 assert values('DETACHED_PIECES')[2]==1
 report[name]=dict(bytes=len(checkpoint),geomod=geomod,material_tokens=tokens)
 print(name,report[name],flush=True)
assert (folder/'resume.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'beam continuation differs'
report.update(result='PASS',scope='Actual beam rocket and exact PC player/destruction continuation; visual and Xbox acceptance separate')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
