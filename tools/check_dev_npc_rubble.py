"""Opt-in authored miner walking through the dry post testbed; process-local only."""
import json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/geomod-postedit-re/dev-npc';folder.mkdir(parents=True,exist_ok=True)
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
recording=folder/'inputs.bin';recording.write_bytes(source[:8+350*48]+bytes(450*48))
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_DEV_NPC='1')
with (folder/'run.log').open('wb') as log:
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(recording),str(folder/'final.ppm')],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert result.returncode==0,'Inspect run.log'
lines=(folder/'run.log').read_text().splitlines()
poses=[list(map(float,line.split()[1:])) for line in lines if line.startswith('DEV_NPC ')]
assert poses and poses[0][0]==0 and poses[-1][0]==780,poses
policy=list(map(float,next(line for line in lines if line.startswith("DEV_NPC_POLICY ")).split()[1:]))
assert policy[0]==9 and 0<policy[1]<=.5,policy
assert poses[-1][4]==0,poses
assert poses[-1][3]<poses[0][3]-3,poses
assert 'Completed 800 frames' in lines[-1],lines[-1]
report=dict(result='PASS',policy=policy,poses=poses,scope='Authored miner1 movement and fragment rejection control: class9 and radius<=0.5 are ineligible; positive admitted contact and native Xbox unverified')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
