"""Unstaged Area2 route reconnaissance; reports observations, not completion."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/area2-spawn-replay';folder.mkdir(exist_ok=True)
def move(i):
    if 30<=i<300 or 460<=i<530:return 0,1
    if 360<=i<415 or 570<=i<740 or 960<=i<1020 or 1160<=i<1205 or 1340<=i<1360:return 1,0
    if 900<=i<925 or 1070<=i<1120 or 1270<=i<1310:return 0,-1
    return 0,0
source=folder/'input.bin'
source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
    struct.pack('<5f7I',move(i)[0],0,move(i)[1],0,0,0,0,1,0,0,0,0) for i in range(2400)))
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
assert 'Completed 2400 frames' in run.stdout
traces=[list(map(float,l.split()[1:])) for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_TRACE ')]
attack=next(l for l in run.stdout.splitlines() if l.startswith('SCRIPT_ATTACK '))
report=dict(status='OBSERVED',traces=traces,attack=attack,
    scope='Real L2S2a player spawn; movement and Use only. The current path reaches the corridor but does not reach rescue trigger5670. No route-completion assertion.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(report['scope']);print(traces[-1])
