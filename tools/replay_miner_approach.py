"""Local walking/Use approach after one placement at the authored enable trigger."""
import json, os, struct, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/miner-approach-replay';folder.mkdir(exist_ok=True)
rows=[]
for name,use,walk in [('use',1,True),('no-use',0,True),('out-of-range',1,False)]:
    source=folder/(name+'.bin')
    def movement(frame):
        if walk and 30<=frame<85:return 0,-1
        if walk and 85<=frame<130:return 1,0
        return 0,0
    source.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(
        struct.pack('<5f7I',movement(i)[0],0,movement(i)[1],0,0,0,0,use,0,0,0,0)
        for i in range(1200)))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRIGGER_UID='5670')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',
        str(root/'Installed_Game'),str(source),str(folder/(name+'.ppm'))],cwd=root,env=env,capture_output=True,text=True)
    (folder/(name+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
    def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
    door,attack,reach=words('ROTATING_DOORS'),words('SCRIPT_ATTACK'),words('USE_REACH')
    assert words('SWITCH_DETAIL')[0]==5671 and 'Completed 1200 frames' in run.stdout
    if use and walk:
        assert door[1:3]==[1,8512] and attack[1:3]==[8496,8490] and attack[5]>0
        assert reach[3]>0 and any(l.startswith('DEATH_WATCH 8611 1 ') for l in run.stdout.splitlines())
        assert attack[4]==0 and struct.unpack('<f',struct.pack('<I',attack[8]))[0]<=0, 'Dead target retained its scripted Attack order'
    else:
        assert door[:2]==[0,0] and attack==[0]*12 and reach[3]==0
    rows.append(dict(case=name,door=door,attack=attack,use_reach=reach));print(rows[-1],flush=True)
(folder/'report.json').write_text(json.dumps(dict(result='PASS',cases=rows,
    scope='One initial placement at trigger5670, then movement and Use only; no forced events. Local approach and negative controls, not full traversal from level spawn.'),indent=2))
