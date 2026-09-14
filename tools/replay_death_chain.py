"""Authored L7S2 watcher5012 -> Goal_Set5011, with contained damage only."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/death-chain';folder.mkdir(exist_ok=True)
results=[]
for frames in (59,90):
 source=folder/f'inputs-{frames}.bin'
 source.write_bytes(b'RFI5'+struct.pack('<I',44)+struct.pack('<5f6I',*([0]*11))*frames)
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_LEVEL='L7S2.rfl',RF_REPLAY_ARCHIVE='levels2.vpp',RF_REPLAY_WATCH_UID='5012')
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'final-{frames}.ppm')],env=env,capture_output=True,text=True)
 (folder/f'pc-{frames}.log').write_text(run.stdout+run.stderr)
 assert run.returncode==0,run.stderr[-1200:]
 lines=run.stdout.splitlines();fired=int(frames>60)
 assert f'MISSION_GOAL door1 {fired} 0' in lines,[x for x in lines if x.startswith('MISSION_GOAL')]
 watches=[x for x in lines if x.startswith('DEATH_WATCH ')]
 assert f'DEATH_WATCH 5012 {fired} {1016 if fired else 0}' in watches,watches
 test=next(x for x in lines if x.startswith('WATCH_TEST '));assert test==('WATCH_TEST 2 4907 60 0' if fired else 'WATCH_TEST 1 4887 30 0'),test
 results.append(dict(frames=frames,goal=fired,watch_test=test,watches=watches))
report=dict(result='PASS',scope='Controlled fatal damage to authored NPCs; real death watcher and goal dispatcher, not a played combat route.',cases=results)
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
