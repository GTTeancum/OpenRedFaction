"""Authored switch9836 enables trigger9840; delayed switch8687 routes to currently unsupported Alarm8686."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/live-switches';folder.mkdir(exist_ok=True);rows=[]
for name,frames,setup in [('before_delay',4,'9836'),('enable',120,'9836'),('limited_repeat',120,'9836,9836'),('alarm_route',520,'8687'),('return',240,'9836')]:
    source=folder/(name+'.bin');source.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(frames*48))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_SETUP_UID=setup)
    if name=='return':env.update(RF_REPLAY_EXIT_UID='9019',RF_REPLAY_RETURN_EXIT_UID='9346')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(s for s in run.stdout.splitlines() if s.startswith(label+' ')).split()[1:]))
    runtime,detail=words('SWITCH_RUNTIME'),words('SWITCH_DETAIL')
    assert runtime[5]==0,(runtime,detail)
    if name=='before_delay':
        assert runtime[3]==0 and detail[:7]==[9836,1,0,1,0,9840,5] and detail[7]&16,(runtime,detail)
    elif name=='return':
        assert words('SWITCH_HISTORY')[:3]==[4,3,0]
        assert detail[:7]==[9836,0,1,1,0,9840,5] and not detail[7]&16,detail
        assert run.stdout.count('LEVEL_TRANSITION ')==2
    elif name=='alarm_route':
        assert runtime[1]>=1 and runtime[3]==1 and detail[:7]==[8687,1,1,1,0,8686,6],(runtime,detail)
    else:assert runtime[3]==1 and detail[:7]==[9836,0,1,1,0,9840,5] and not detail[7]&16,detail
    assert f'Completed {frames} frames' in run.stdout
    rows.append(dict(case=name,runtime=runtime,detail=detail));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Authored switches dispatched through process-local setup, with live target flags, delay and activation limit. Round trip restores switch state and linked trigger flags. Alarm action and other target families remain open.'),indent=2))
