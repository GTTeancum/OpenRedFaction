"""Reset an eight-cut DEV room and verify a fresh persistent lightmap owner."""
import os
from pathlib import Path
import struct
import subprocess
from dev_destruction_check import ROOT

def main():
    folder=ROOT/'artifacts/destruction'
    data=(folder/'eight.bin').read_bytes()
    assert data[:8]==b'RFI6'+struct.pack('<I',48) and len(data)==8+1500*48
    for frame in range(1500,1800):
        data+=struct.pack('<5f7I',0,0,0,0,0,int(frame==1520),0,int(frame==1520),int(frame==1550),0,0,int(frame==1520))
    path=folder/'noise-reset.bin';path.write_bytes(data)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(path.with_suffix('.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
    path.with_suffix('.log').write_text(result.stdout+result.stderr);result.check_returncode()
    row=lambda label:list(map(int,next(l.split()[1:] for l in result.stdout.splitlines() if l.startswith(label+' '))))
    terrain,noise,atlas,bake=row('GEOMOD'),row('TERRAIN_NOISE'),row('TERRAIN_ATLAS'),row('TERRAIN_BAKE')
    assert terrain[1:3]==[1,11],terrain
    assert noise[0]==1 and 0<noise[1]<100 and noise[1]==noise[5] and noise[4]==0 and noise[7]==11,noise
    assert bake[0]==noise[3]==atlas[5] and bake[3]==0 and bake[4]==11,(noise,atlas,bake)
    assert row('PLAYER_LIFE')[0]==0
    print('PASS: eight cuts, guarded reset, fresh cut and completed replacement lightmaps',noise)

if __name__=='__main__':main()
