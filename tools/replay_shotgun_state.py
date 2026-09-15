"""Authored ammo pickups, reload timing and shotgun level persistence.
Process-local placement and exit dispatch are explicit; no inventory injection.
"""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/shotgun-state';folder.mkdir(parents=True,exist_ok=True)
results=[]
for name,frames in [('manual_before',219),('manual',221),('automatic',360),('return',240)]:
    returning=name=='return';automatic=name=='automatic'
    source=folder/(name+'.bin')
    source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
        struct.pack('<5f7I',0,0,float(10<=i<25 or (returning and 191<=i<206)),0,0,0,0,0,
            int(not automatic and i==50),int(not returning and not automatic and i==100),int(i==40),int(automatic and 50<=i<180))
        for i in range(frames)))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S3.rfl' if returning else 'L7S4.rfl',RF_REPLAY_ARCHIVE='levels1.vpp' if returning else 'levels2.vpp',RF_REPLAY_ITEM_UID='2115' if returning else '11087',RF_REPLAY_TRACE='1')
    if returning:env.update(RF_REPLAY_EXIT_UID='5151',RF_REPLAY_RETURN_EXIT_UID='5150',RF_REPLAY_RETURN_ITEM_UID='2115')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
    ammo,combat,shotgun,pickups=map(words,['PLAYER_AMMO','COMBAT','SHOTGUN','PICKUPS'])
    assert ammo[7]==combat[7]==shotgun[6]==pickups[7]==0
    assert words('PLAYER_LIFE')[0]==0 and words('WEAPON_SELECTION')[0]==3
    assert f'Completed {frames} frames' in run.stdout
    if returning:
        assert ammo[:5]==[5,0,7,0,0] and pickups[3]==0
        assert 'TAKEN_PICKUP l2s3.rfl 2115' in run.stdout
        transitions=[l.split()[1:] for l in run.stdout.splitlines() if l.startswith('LEVEL_TRANSITION ')]
        assert transitions==[['L2S3.rfl','L2S2a.rfl','5151','61'],['L2S2a.rfl','L2S3.rfl','5150','181']]
    else:
        assert pickups[3:6]==[6,56,11087]
        if name=='manual_before':assert ammo[:5]==[5,32,7,0,0] and combat[6]==2
        elif name=='manual':assert ammo[:5]==[5,31,8,1,1] and combat[6]==0
        else:assert ammo[:5]==[5,24,8,8,1] and shotgun[:5]==[8,32,0,0,8]
    results.append(dict(case=name,frames=frames,ammo=ammo,shotgun=shotgun,pickups=pickups));print(results[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=results,scope='Actual pickup contacts after staging; manual/automatic reload and two dispatched authored exits. Full route and all campaign persistence not claimed.'),indent=2))
