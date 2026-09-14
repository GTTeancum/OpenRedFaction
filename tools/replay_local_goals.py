"""Authored sample1 goal across L8S1/L8S2/back, with fresh-process control."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/local-goals';folder.mkdir(exist_ok=True)
source=folder/'return.bin';source.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(240*48))
rows=[]
for name,setter in [('set',True),('fresh',False)]:
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L8S1.rfl',RF_REPLAY_ARCHIVE='levels2.vpp',RF_REPLAY_EXIT_UID='5625',RF_REPLAY_RETURN_EXIT_UID='5623')
    if setter:env['RF_REPLAY_GOAL_UID']='8887'
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    transitions=[s for s in run.stdout.splitlines() if s.startswith('LEVEL_TRANSITION ')]
    goals=[s for s in run.stdout.splitlines() if s.startswith('MISSION_GOAL ')]
    assert transitions==['LEVEL_TRANSITION L8S1.rfl L8S2.rfl 5625 61','LEVEL_TRANSITION L8S2.rfl L8S1.rfl 5623 181'],transitions
    assert f'MISSION_GOAL sample1 {int(setter)} 0' in goals,goals
    assert 'MISSION_GOAL VAT 0 1' in goals,goals
    assert 'Completed 240 frames' in run.stdout
    rows.append(dict(case=name,transitions=transitions,goals=goals));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Authored goal setter and section exits with process-local dispatch; local value restored, global counter unchanged, new process reset. Not a walked campaign route.'),indent=2))
