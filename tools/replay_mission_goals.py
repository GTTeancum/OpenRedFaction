"""Authored VAT setter and forced lab exit; no host input or pathfinding claim."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/mission-replay';folder.mkdir(exist_ok=True)
source=folder/'idle.bin';source.write_bytes(b'RFI5'+struct.pack('<I',44)+bytes(120*44))
rows=[]
for name,setter in [('set',True),('fresh',False)]:
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L8S1.rfl',RF_REPLAY_ARCHIVE='levels2.vpp',RF_REPLAY_EXIT_UID='5625')
    if setter:env['RF_REPLAY_GOAL_UID']='8898'
    result=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.stderr[-1000:])
    transitions=[x for x in result.stdout.splitlines() if x.startswith('LEVEL_TRANSITION ')]
    goals=[x for x in result.stdout.splitlines() if x.startswith('MISSION_GOAL ')]
    assert transitions==['LEVEL_TRANSITION L8S1.rfl L8S2.rfl 5625 61'],transitions
    assert f'MISSION_GOAL VAT {int(setter)} 1' in goals,goals
    assert not any(x.startswith('MISSION_GOAL sample1 ') for x in goals),goals
    rows.append(dict(case=name,transitions=transitions,goals=goals))
(folder/'report.json').write_text(json.dumps(dict(result='PASS',results=rows),indent=2)+'\n')
print('PASS authored lab mission counter handoff, local counter retirement and fresh campaign reset')
