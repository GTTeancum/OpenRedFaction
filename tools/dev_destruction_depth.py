"""Close/oblique GeoMod views using only process-contained game replay.

Checks publication, survival and that all cameras remain in the original
room; appearance must be inspected in each output separately.
"""
import json,os,struct,subprocess
from pathlib import Path
from PIL import Image
from dev_destruction_check import recording
ROOT=Path(__file__).resolve().parents[1]

def main():
    folder=ROOT/'artifacts/destruction/depth';folder.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    rows=[]
    for name,strafe,yaw in [('close',0,0),('left',-.4,.22),('right',.4,-.22)]:
        data=recording('approach')+b''.join(struct.pack('<5f7I',0,0,.65,0,0,0,0,0,0,0,0,0) for _ in range(50))
        data+=b''.join(struct.pack('<5f7I',strafe,0,0,0,yaw,0,0,0,0,0,0,0) for _ in range(45))
        path=folder/(name+'.bin');path.write_bytes(data)
        r=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(path),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,capture_output=True,text=True)
        log=r.stdout+r.stderr;(folder/(name+'.log')).write_text(log);r.check_returncode()
        row=lambda label:next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))
        bake=list(map(int,row('TERRAIN_BAKE')))
        assert bake[1]<=64 and bake[2]<=64,bake
        atlas=list(map(int,row('TERRAIN_ATLAS')))
        assert atlas[:3]==[1,512,512] and 0<atlas[3]<=1280*1024,atlas
        terrain=list(map(int,row('GEOMOD')));position=list(map(float,row('CAMPAIGN_FINAL_POSITION')))
        assert terrain[:3]==[1,2,3] and terrain[5:]==[0,2,2] and terrain[4]<=1024*1024+65536,terrain
        assert list(map(int,row('ROCKETS')))==[2,2,0,0,2,0,0,0]
        assert int(row('PLAYER_LIFE')[0])==0
        shadows=list(map(int,row('TERRAIN_SHADOWS')))
        assert shadows[0]==2 and 0<shadows[2]<shadows[1]<100000 and shadows[3]>300,shadows
        assert -16<position[0]<-10 and -12<position[1]<-10 and 8<position[2]<13,position
        Image.open(folder/(name+'.ppm')).save(folder/(name+'.png'))
        rows.append(dict(view=name,frames=(len(data)-8)//48,position=position,terrain=terrain,shadows=shadows))
        print('PASS:',name,position)
    (folder/'report.json').write_text(json.dumps(dict(scope=__doc__,cases=rows),indent=2)+'\n')

if __name__=='__main__':main()
