"""Exercise the current eight-cut history limit, render subdivision and settling.

Ammo refill and firing are delivered through the process-local replay only.
This verifies one bounded cut pattern, not arbitrary campaign destruction.
"""
import os,struct,subprocess
from dev_destruction_check import ROOT,recording

def main():
    folder=ROOT/'artifacts/destruction';folder.mkdir(parents=True,exist_ok=True)
    data=bytearray(recording('six')+bytes(48)*700)
    for frame in (770,880):struct.pack_into('<I',data,8+frame*48+32,1)
    for offset in (28,36):struct.pack_into('<I',data,8+740*48+offset,1)
    path=folder/'eight.bin';path.write_bytes(data)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(path.with_suffix('.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
    path.with_suffix('.log').write_text(r.stdout+r.stderr);r.check_returncode()
    row=lambda name:list(map(int,next(l.split()[1:] for l in r.stdout.splitlines() if l.startswith(name+' '))))
    terrain,draw,bake,atlas=row('GEOMOD'),row('TERRAIN_DRAW'),row('TERRAIN_BAKE'),row('TERRAIN_ATLAS')
    assert terrain[:3]==[1,8,9] and terrain[5:]==[0,8,8] and terrain[4]<=1024*1024+65536,terrain
    assert 0<draw[0]<=draw[1]<=8192 and draw[2]==draw[1]-draw[0] and draw[3]<=320*1024 and draw[4]==9,draw
    assert bake[0]==atlas[5] and bake[1]==bake[3]==0 and bake[2]<=512*512 and bake[4]==9,(bake,atlas)
    noise=row('TERRAIN_NOISE')
    assert noise[0]==1 and 0<noise[1]<=1024 and noise[1]==noise[5] and noise[2]>0 and noise[3]==atlas[5] and noise[4]>0 and noise[6]<=128*1024 and noise[7]==9,noise
    assert atlas[3]<=1280*1024,atlas
    assert row('PLAYER_LIFE')[0]==0 and row('DEBRIS')[1]==0
    print('PASS:1500 frames, eight cuts, complete lightmap bake; render counters',draw)
if __name__=='__main__':main()
