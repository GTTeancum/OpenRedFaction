"""Verify live retirement releases resources without losing checkpoint identity.
Run check_detached_rocket.py first to create the ordinary two-rocket saved state.
"""
import json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-postedit-re/retired-resources';folder.mkdir(parents=True,exist_ok=True)
rocket=ROOT/'artifacts/geomod-postedit-re/detached-rocket';source=(rocket/'inputs.bin').read_bytes();idle=source[:8]+bytes(201*48)
recordings={'healthy':(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes(),'continued':idle,'control':source[:-48]+idle[8:]}
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
report={}
for name,data in recordings.items():
 path=folder/name;path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp')))
 if name=='continued':local['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(rocket/'saved.rfcp')
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=local,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,name
 line=next(x for x in path.with_suffix('.log').read_text().splitlines() if x.startswith('DETACHED_PIECES '));pieces=list(map(int,line.split()[1:]));assert pieces[5]==0
 assert pieces[2]==(1 if name=='healthy' else 0),(name,pieces)
 report[name]=dict(pieces=pieces)
assert (folder/'continued.rfcp').read_bytes()==(folder/'control.rfcp').read_bytes(),'retired continuation differs'
live=report['healthy']['pieces'][4];dead=report['control']['pieces'][4];assert dead<live/10,(live,dead)
report.update(result='PASS',released_bytes=live-dead,scope='Healthy retention, real rocket retirement, collected save reload, and byte-exact uninterrupted continuation; no healthy expiry.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
