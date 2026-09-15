"""Continue the uninterrupted shaft route into L3S1's entry side room."""
import argparse,json,os,struct,subprocess
from pathlib import Path
from replay_area3_main_shaft import build_exit_walk
root=Path(__file__).resolve().parents[1]

def build_input(side_room=False):
    data=bytearray(build_exit_walk())
    for i in range(13200,13700 if side_room else 13870):
        if i>=13700:
            value=(0,float(13745<=i<13779),float(13710<=i<13732),0,.98 if 13750<=i<13762 else 0,0,0,1,0,0,0,int(13800<=i<13835))
        elif i<13480:
            value=(0,0,float(13310<=i<13440),0,-.98 if i<13296 else 0,0,0,1,0,0,0,0)
        else:
            x=-1 if 13515<=i<13555 or 13605<=i<13620 else 0
            z=1 if 13490<=i<13508 or 13575<=i<13597 else 0
            value=(x,0,z,0,0,0,0,1,0,0,0,0)
        data.extend(struct.pack('<5f7I',*value))
    return data

def verify(log,side_room=False):
    frames=13700 if side_room else 13870
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    assert f'Completed {frames} frames' in log and words('PLAYER_LIFE')[0]==0
    transitions=[l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]
    assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275'],['L2S3.rfl','L3S1.rfl','6604','13085']]
    assert words('PLAYER_AMMO')[:3]==[8,0,5 if side_room else 0] and words('COMBAT')[:3]==([0,0,0] if side_room else [5,2,1])
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==5
    rows={int(l.split()[1]):l.split() for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert float(rows[789][-1])==100
    if side_room:assert float(rows[109][-1])==100
    else:assert float(rows[109][-1])<=0
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    if side_room:assert -43.8<position[0]<-43.6 and -3.2<position[1]<-3 and -15.8<position[2]<-15.5
    else:assert -41.7<position[0]<-41.4 and .2<position[1]<.4 and -15.8<position[2]<-15.5 and words('PLAYER_CLIMB')[4]==2
    return dict(result='PASS',frames=frames,position=position,health=health,
        scope=('Uninterrupted route clears L3S1 guard109 from the ladder; allied miner789 unharmed. Health and ammunition supplies remain open.' if not side_room else 'Uninterrupted route into L3S1 entry side room; miner789 unharmed, five rifle rounds retained. Ladder climb, guard109 and supplies remain open.'))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--side-room',action='store_true');args=parser.parse_args()
    folder=root/('artifacts/area3-entry-side' if args.side_room else 'artifacts/area3-entry-guard');folder.mkdir(parents=True,exist_ok=True)
    source=folder/'input.bin';source.write_bytes(build_input(args.side_room))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(folder/'run.log').write_text(log);run.check_returncode()
    report=verify(log,args.side_room);(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)

if __name__=='__main__':main()
