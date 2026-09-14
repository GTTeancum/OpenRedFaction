"""Authored L1S1 baton9463: view, acquire, repeat ownership and retired-item revisit."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/riot-pickups';folder.mkdir(exist_ok=True)
rows=[]
for name,frames in [('view',32),('collect',150),('owned',150),('return',240)]:
    source=folder/(name+'.bin')
    source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
        struct.pack('<5f7I',0,0,float(name!='view' and (10<=i<25 or (name=='return' and 191<=i<206))),0,0,0,0,0,0,0,0,False)
        for i in range(frames)))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ITEM_UID='9463')
    if name=='owned':env['RF_REPLAY_SETUP_UID']='9870'
    if name=='return':env.update(RF_REPLAY_EXIT_UID='9019',RF_REPLAY_RETURN_EXIT_UID='9346',RF_REPLAY_RETURN_ITEM_UID='9463')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(s for s in run.stdout.splitlines() if s.startswith(label+' ')).split()[1:]))
    ammo,pickups,model=map(words,('PLAYER_AMMO','PICKUPS','PLAYER_WEAPON'))
    assert ammo[7]==pickups[7]==model[6]==0,(ammo,pickups,model)
    if name=='view':assert pickups[3]==0 and pickups[6]>0 and ammo[0]==0xffffffff,(pickups,ammo)
    elif name=='return':
        assert ammo[:3]==[2,0,100] and pickups[3]==0,(ammo,pickups)
        assert words("STARTUP_INVENTORY")[:3]==[1,1,0]
        assert 'TAKEN_PICKUP l1s1.rfl 9463' in run.stdout
        assert run.stdout.count('LEVEL_TRANSITION ')==2
    else:
        assert pickups[3:6]==[1,100,9463] and ammo[:3]==[2,100 if name=='owned' else 0,100],(pickups,ammo)
        assert model[2]>0 and model[4]<=1024*1024
    assert f'Completed {frames} frames' in run.stdout
    rows.append(dict(case=name,ammo=ammo,pickups=pickups,model=model));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,scope='Authored world item, process-local player placement/exit dispatch, actual contact and persistent retirement. No completed traversal claim.'),indent=2))
