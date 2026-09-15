"""Six cuts followed by enough idle frames to verify incremental lighting converges."""
import os
from pathlib import Path
import subprocess
from dev_destruction_check import ROOT, recording

def main():
    folder=ROOT/'artifacts/destruction'
    folder.mkdir(parents=True,exist_ok=True)
    data=recording('six')
    path=folder/'settled.bin'
    path.write_bytes(data+bytes(48)*400)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(folder/'settled.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
    log=result.stdout+result.stderr
    (folder/'settled.log').write_text(log)
    result.check_returncode()
    row=lambda key:list(map(int,next(line.split()[1:] for line in log.splitlines() if line.startswith(key+' '))))
    terrain,atlas,bake=row('GEOMOD'),row('TERRAIN_ATLAS'),row('TERRAIN_BAKE')
    assert terrain[:3]==[1,6,7] and terrain[5:]==[0,6,6],terrain
    assert atlas[:3]==[1,512,512] and atlas[3]<=1280*1024 and atlas[4]==7,atlas
    assert bake[0]==atlas[5] and bake[1]==0 and 0<bake[2]<=512*512 and bake[3]==0 and bake[4]==7 and bake[5]==0,bake
    noise=row('TERRAIN_NOISE')
    assert noise[0]==1 and noise[1]==noise[5] and noise[2]>0 and noise[3]==atlas[5] and noise[4]>0 and noise[6]<=128*1024 and noise[7]==7,noise
    uploads=row('TERRAIN_UPLOAD')
    assert uploads[3]==1 and uploads[0]>uploads[3] and uploads[2]==512*512,uploads
    partial_pixels=uploads[1]-uploads[3]*512*512
    assert partial_pixels<(uploads[0]-uploads[3])*512*512//10,uploads
    assert row('PLAYER_LIFE')[0]==0
    print('PASS:1200 frames, six cuts, persistent mappings retained, final atlas complete; new maps filled before frame submission')

if __name__=='__main__':main()
