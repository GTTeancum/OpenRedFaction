"""Capture successive close-range DEV debris states; inspect images separately.

Shared scripted input stays inside the game. Runtime debris motion is a
provisional integration, not a claim of original physics/appearance parity.
"""
from pathlib import Path
import os,struct,subprocess
from dev_destruction_check import ROOT,recording

def main():
    folder=ROOT/'artifacts/destruction/debris';folder.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    hashes=[]
    for frame in (560,580,620,900):
        data=recording('approach')+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==520),0,0,0) for i in range(500,frame))
        path=folder/f'close-{frame}.bin';path.write_bytes(data)
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(path.with_suffix('.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
        path.with_suffix('.log').write_text(r.stdout+r.stderr);r.check_returncode()
        row=lambda label:next(l.split()[1:] for l in r.stdout.splitlines() if l.startswith(label+' '))
        debris=list(map(int,row('DEBRIS')));terrain=list(map(int,row('GEOMOD')))
        assert terrain[1]==3 and debris[0]==48 and debris[6]<=65536 and debris[7]==0,(frame,terrain,debris)
        assert int(row('PLAYER_LIFE')[0])==0
        if frame<900:
            assert 0<debris[1]<=80 and debris[2]>0 and debris[4]>0,(frame,debris)
            hashes.append(debris[5])
        else:assert debris[1]==debris[4]==0 and debris[3]==48,debris
        print('PASS:',frame,'frames',debris)
    assert len(set(hashes))==len(hashes),'Expected changing visible debris geometry'
if __name__=='__main__':main()
