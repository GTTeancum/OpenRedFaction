"""Opt-in, headless L5S2 lava exposure versus same-room dry control.
Uses authored miner armor/health and damage behavior; no invulnerability/input injection.
"""
import hashlib,json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'artifacts/lava-gameplay';folder.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/pc/Release/rf_pc_play.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();results={}
    for selector,name in ((2,'lava'),(3,'dry')):
        for frames in (2,30,120,240):
            key=f'{name}-{frames}';recording=folder/(key+'.bin');statefile=folder/(key+'.state')
            recording.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(frames*48))
            env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
            env.update(RF_REPLAY_SWIM_TEST=str(selector),RF_REPLAY_LEVEL='L5S2.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_PLAYER_STATE_OUT=str(statefile))
            run=subprocess.run([str(exe),'--spawn-replay',str(ROOT/'Installed_Game'),str(recording),str(folder/(key+'.ppm'))],env=env,cwd=ROOT,capture_output=True,text=True,timeout=180)
            (folder/(key+'.log')).write_text(run.stdout+run.stderr);run.check_returncode()
            def row(label):return list(map(int,next(line for line in run.stdout.splitlines() if line.startswith(label+' ')).split()[1:]))
            hazard=row('LIQUID_DAMAGE');swim=row('PLAYER_SWIM');enemy=row('ENEMY_COMBAT')
            health,armor=struct.unpack_from('<2f',statefile.read_bytes(),len(statefile.read_bytes())-16)
            assert hazard[0]==frames-1 and hazard[7]==0 and hazard[2:5]==[13,2,3],(key,hazard)
            assert enemy[1:4]==[0,0,0] and swim[11]==0,(key,enemy,swim)
            if selector==2:
                assert hazard[1]==frames-1,(key,hazard)
                # Two rendered frames include exactly the initialized first physics tick.
                if frames==2:assert hazard[0:5]==[1,1,13,2,3],(key,hazard)
                expected=(frames-1)*35/60
                assert abs((200-health-armor)-expected)<.004,(key,health,armor,expected)
                assert swim[1]==1,(key,swim)
                if frames==240:assert armor==0 and health<100,(key,health,armor)
                else:assert health==100 and armor<100,(key,health,armor)
            else:assert hazard[1]==0 and health==100 and armor==100 and swim[1]==0,(key,hazard,health,armor,swim)
            results[key]=dict(frames=frames,hazard=hazard,swim=swim,health=health,armor=armor)
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==digest
    (folder/'verification.json').write_text(json.dumps(dict(result='PASS',pc_sha256=digest,cases=results,scope='Authored L5S2 room13 lava and dry platform; emitted requests and armor/health loss measured per tick. Numerical gameplay acceptance; output visual inspection separate. No acid-authored-level or Xbox claim.'),indent=2))
    print('PASS authored lava first-tick/35per-second armor then health and dry control')
if __name__=='__main__':main()
