"""Authored ambient delay boundary and full-table failure campaign checkpoints."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/ambient-schedule-replay';folder.mkdir(exist_ok=True)
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
results=[]
for level,frames,occupied,pending in [('L4S2.rfl',30,6,1),('L4S2.rfl',31,7,0),('L1S2.rfl',6,25,1),('L1S2.rfl',7,25,0)]:
 source=folder/f'{level}-{frames}.bin';source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(frames*32))
 env.update(RF_REPLAY_LEVEL=level,RF_REPLAY_ARCHIVE='levels1.vpp')
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'{level}-{frames}.ppm')],env=env,capture_output=True,text=True)
 (folder/f'{level}-{frames}.txt').write_text(run.stdout+run.stderr);run.check_returncode()
 state=list(map(int,next(s for s in run.stdout.splitlines() if s.startswith('AMBIENT_SCHEDULE ')).split()[1:]))
 assert state[:4]==[frames,(frames-1)*1000//60,occupied,pending],(level,frames,state)
 results.append(dict(level=level,frames=frames,state=state))
assert results[0]['state'][4:]!=results[1]['state'][4:]
assert results[2]['state'][4]!=results[3]['state'][4] and results[2]['state'][5]==results[3]['state'][5]
report=dict(result='PASS',results=results,scope='Authored L4S2 UID1519 starts at500ms, after483ms pending checkpoint. L1S2 UID9925 expires at100ms into a full25-slot table, clears its timer and leaves the table unchanged. No staged input; shared ambient playback is active, without device capture or control-slot recycling.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
