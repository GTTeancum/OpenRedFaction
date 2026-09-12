"""Check live campaign model registration and retirement on PC replays."""
import json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/campaign-model-registry';folder.mkdir(exist_ok=True)
results=[]
for level,inputs,staged,total,loaded in [('L1S1.rfl','door-audio-reference/inputs.bin','RF_REPLAY_DOOR_START',78,78),('L1S2.rfl','lift-cycle/inputs.bin','RF_REPLAY_LIFT_START',39,38),('L1S3.rfl','npc-bodies-start.bin',None,28,25)]:
 env=os.environ.copy()
 for key in ('RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_DAMAGE_UID','RF_REPLAY_REGION_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
 env.update(RF_REPLAY_LEVEL=level,RF_REPLAY_ARCHIVE='levels1.vpp')
 if staged:env[staged]='1'
 result=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(root/'artifacts'/inputs),str(folder/(level+'.ppm'))],cwd=root,env=env,capture_output=True,text=True,check=True)
 (folder/(level+'.txt')).write_text(result.stdout)
 rows={line.split()[0]:list(map(int,line.split()[1:])) for line in result.stdout.splitlines() if line.startswith(('NPC_MODELS ','NPC_BODIES '))}
 assert rows['NPC_MODELS']==[loaded,total*20,loaded,0],(level,rows)
 assert rows['NPC_BODIES'][0:2]==[total,loaded]
 results.append(dict(level=level,models=rows['NPC_MODELS'],bodies=rows['NPC_BODIES']))
 print(level,rows['NPC_MODELS'],flush=True)
report=dict(result='PASS',levels=results,scope='PC campaign register/retire through retained poses and live shared motion references. All loaded actors registered, then retired before level resource teardown.32KiB bounded registry allocation;20 bytes per authored slot. No corpse transfer or native XEMU claim.')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
