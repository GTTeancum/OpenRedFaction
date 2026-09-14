"""Ensure NPC death releases custom scripted animation ownership."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/animation-interrupt-replay';folder.mkdir(exist_ok=True)
source=folder/'idle.bin';source.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(48*180))
rows=[]
for name,setup,ticks,cancels in [('natural',None,150,0),('death','7201,7196',31,1)]:
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env['RF_REPLAY_LEVEL']='L2S1.rfl'
    if setup:env['RF_REPLAY_SETUP_UID']=setup
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(line for line in run.stdout.splitlines() if line.startswith(label+' ')).split()[1:]))
    animation=words('SCRIPT_ANIMATION');slay=words('SCRIPT_SLAYS');death=words('COMBAT_DEATH')
    assert animation==[1,1,1,0,0,7201,7192,ticks,0,cancels],animation
    if setup:assert slay[:3]==[1,1,7192] and slay[4:]==[1000,0] and death[0]==1,(slay,death)
    else:assert slay==[0]*6 and death[0]==0
    assert 'Completed 180 frames' in run.stdout
    rows.append(dict(case=name,animation=animation,slay=slay,death=death));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Natural delayed cower control and explicit authored Slay at frame60; verifies one cancellation and unchanged death-entry count, not full AI interruption coverage.'),indent=2))
