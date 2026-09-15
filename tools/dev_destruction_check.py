"""Ordinary rocket input: visible crater, repeated excavation and traversal.

Only targeted game replay; inspect the resulting frames separately.
"""
import os
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def recording(name):
    frames=500 if name=='approach' else 620 if name=='walk' else 400 if name=='double' else 260
    shots=() if name=='before' else (110,) if name=='single' else (110,220)
    return b'RFI6'+struct.pack('<I',48)+b''.join(
        struct.pack('<5f7I',.3 if name=='approach' and 400<=i<450 else 0,0,.8 if (name=='walk' and i>=350) or (name=='approach' and 350<=i<480) else 0,0,.7 if i<90 else 0,
                    0,0,0,int(i in shots),0,int(i in (10,20,30,40)),0) for i in range(frames))
def main():
    folder=ROOT/'artifacts/destruction';folder.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env['RF_REPLAY_TRACE']='1'
    for name in ('before','single','double','walk','approach'):
        path=folder/(name+'.bin');path.write_bytes(recording(name))
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),
            str(path),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
        log=r.stdout+r.stderr;(folder/(name+'.log')).write_text(log);r.check_returncode()
        def row(label):return next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))
        cuts=0 if name=='before' else 1 if name=='single' else 2
        terrain=list(map(int,row('GEOMOD')))
        assert terrain[:3]==[1,cuts,1+cuts] and terrain[5:]==[0,cuts,cuts],terrain
        assert terrain[4]<=1024*1024+65536
        assert list(map(int,row('ROCKETS')))==[cuts,cuts,0,0,cuts,0,0,0]
        assert int(row('PLAYER_LIFE')[0])==0
        position=list(map(float,row('CAMPAIGN_FINAL_POSITION')))
        if name=='walk':assert position[0]<-18 and -14<position[1]<-12,position
        elif name=='approach':assert -10<position[0]<-7,position
        else:assert abs(position[0]-.296062)<.003,position
        print('PASS:',name)
if __name__=='__main__':main()
