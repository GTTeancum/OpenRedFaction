"""Return from the L3S1 control booth to the lower checkpoint hall."""
import argparse,json,os,struct,subprocess
from pathlib import Path
from replay_l3s1_entry import build_input as entry_input
root=Path(__file__).resolve().parents[1]

def build_input(passage=False,combat=False):
    passage=passage or combat
    data=bytearray(entry_input(checkpoint=True))
    for i in range(14620,15290 if combat else 15220 if passage else 15060):
        if i>=15220:
            data.extend(struct.pack('<5f7I',0,0,0,0,.98 if i<15227 else 0,0,0,0,0,0,0,int(15228<=i<15260)))
            continue
        if i>=15060:
            x,y,z=0,0,float(15070<=i<15190)
        elif i>=14760:
            x=float(14900<=i<14957);y=-float(14800<=i<14845)
            z=-1 if 14765<=i<14783 or 14860<=i<14883 else int(14970<=i<15000)
        else:
            x=-1 if 14630<=i<14650 else 0;y=-0.0;z=-1 if 14665<=i<14745 else 0
        data.extend(struct.pack('<5f7I',x,y,z,0,0,0,0,0,0,0,0,0))
    return data

def verify(log,passage=False,combat=False):
    passage=passage or combat
    frames=15290 if combat else 15220 if passage else 15060
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    assert f'Completed {frames} frames' in log and words('PLAYER_LIFE')[0]==0
    transitions=[l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]
    assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275'],['L2S3.rfl','L3S1.rfl','6604','13085']]
    assert words('PLAYER_AMMO')[:3]==[8,0,36 if combat else 42] and words('COMBAT')[:3]==([11,4,2] if combat else [5,2,1])
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==(45 if passage else 55)
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    if passage:assert -30.2<position[0]<-29.8 and -3.2<position[1]<-3 and -21.3<position[2]<-20.9
    else:assert -42.1<position[0]<-41.7 and -3<position[1]<-2.7 and -21.3<position[2]<-20.9
    climb=words('PLAYER_CLIMB');assert climb[1:3]==[2,2] and climb[4]==1
    rows={int(l.split()[1]):l.split() for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert float(rows[789][-1])==100 and float(rows[109][-1])<=0
    if combat:assert float(rows[892][-1])<=0
    return dict(result='PASS',frames=frames,health=health,position=position,
        scope=('Uninterrupted route clears checkpoint guard892 after one10-damage hit;45health and36rifle rounds remain.' if combat else 'Uninterrupted route passes the lower checkpoint door alive; guard892 lands one10-damage shot.' if passage else 'Uninterrupted route returns down the booth ladder into the lower hall with55health and42loaded rifle rounds. Checkpoint passage and further combat remain open.'))

def main():
    parser=argparse.ArgumentParser(description=__doc__);route=parser.add_mutually_exclusive_group()
    route.add_argument('--lower-hall',action='store_true');route.add_argument('--passage',action='store_true');args=parser.parse_args()
    combat=not args.lower_hall and not args.passage
    folder=root/('artifacts/l3s1-checkpoint-combat' if combat else 'artifacts/l3s1-checkpoint-passage' if args.passage else 'artifacts/l3s1-lower-hall-v2');folder.mkdir(parents=True,exist_ok=True)
    source=folder/'input.bin';source.write_bytes(build_input(args.passage,combat))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(folder/'run.log').write_text(log);run.check_returncode()
    report=verify(log,args.passage,combat);(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)

if __name__=='__main__':main()
