"""Authored L3S3 auto trigger -> Delay -> Invert -> force off, no staging/input."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/force-event-replay';folder.mkdir(exist_ok=True)
env=dict(os.environ)
for key in ('RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L3S3.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
results=[]
for frames in (2,29,30,31,60):
 source=folder/f'inputs-{frames}.bin';source.write_bytes(b'RFI3'+struct.pack('<I',32)+bytes(frames*32))
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/f'final-{frames}.ppm')],env=env,capture_output=True,text=True,check=True)
 (folder/f'pc-{frames}.txt').write_text(run.stdout)
 def words(label):return list(map(int,next(l for l in run.stdout.splitlines() if l.startswith(label+' ')).split()[1:]))
 state=words('FORCE_STATE');ticks=words('CAMPAIGN_EVENT_TICKS')
 assert state[:2]==[1,1 if ticks[1]<500 else 0],(frames,state,ticks)
 results.append(dict(frames=frames,force_state=state,event_ticks=ticks))
assert results[0]['force_state'][1]==1 and results[-1]['force_state'][1]==0
report=dict(result='PASS',level='L3S3.rfl',authored_chain=[9806,9807,9808,9809,9805],results=results,scope='Authored spawn and automatic trigger, half-second Delay then Invert then Push_Region_State disables sole region. No staging or input. Shared PC campaign checkpoints; XEMU validation separate.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
