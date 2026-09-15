"""Continue the uninterrupted Area2 route into the L2S3 main shaft.
Normal replay inputs only: no staged pose, health, inventory or forced exits.
"""
import argparse,json,math,os,struct,subprocess
from pathlib import Path
from replay_area3_shaft import build_input as landing_input
root=Path(__file__).resolve().parents[1]

def build_input(crossing=False):
    frames=12110 if crossing else 11950
    data=bytearray(landing_input(True))
    for i in range(10950,frames):
        if crossing and i>=11595:
            x=z=0
            if 11625<=i<11633:x,z=.2264,-.974
            if 11645<=i<11669 or 11900<=i<11940:x,z=-.974,-.2264
            if 11690<=i<11747 or 11820<=i<11872 or 12000<=i<12057:x,z=-.2264,.974
            if 11760<=i<11800 or 12070<=i<12095:x,z=.974,.2264
            value=(x,float(11600<=i<11620),z,0,0,0,0,1,0,0,0,0)
        elif i<11050:
            yaw=2.7138+.98/60*min(61,max(0,i-10966))
            x,z=(-.9099,.41483) if i<10965 else ((math.sin(yaw),-math.cos(yaw)) if 10990<=i<11010 else (0,0))
            value=(x,0,z,.98 if 10966<=i<11010 else 0,.98 if 10966<=i<11027 else 0,0,0,1,0,0,int(i in (10951,10953)),int(11028<=i<11042))
        elif i<11490:
            x,z=(.982,-.19) if 11175<=i<11208 else ((.19,.982) if 11220<=i<11242 else (0,0))
            value=(x,float(11245<=i<11800),z,-.98 if i<11118 else 0,-.98 if i<11097 else 0,0,int(i==11245),1,0,0,0,int(11120<=i<11150))
        else:
            value=(0,float(i<11576 or 11610<=i<11900),0,.98 if i<11522 else 0,-.98 if i<11560 else 0,0,0,1,0,0,0,int(11576<=i<11595))
        data.extend(struct.pack('<5f7I',*value))
    return data

def verify(log,crossing=False):
    frames=12110 if crossing else 11950
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    assert f'Completed {frames} frames' in log and words('PLAYER_LIFE')[0]==0
    assert [l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
    assert words('COMBAT')[:3]==[34,24,7] and words('RIFLE_ALT')[:3]==[12,6,3]
    assert words('PLAYER_AMMO')[:3]==[8,0,30] and words('SHOTGUN')[:5]==[3,12,5,1,3]
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==5
    rows={int(l.split()[1]):l.split()[1:] for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert all(float(rows[u][-1])<=0 for u in [2020,2047,1751,2114,1773,2067,1757])
    assert float(rows[2061][-1])==100
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    climb=words('PLAYER_CLIMB');assert climb[1]>0
    if crossing:assert 110<position[0]<111 and 35.7<position[1]<36.2 and 60.5<position[2]<61.5 and climb[3:5]==[3,2]
    else:assert 97<position[0]<98 and 49.8<position[1]<50.5 and 61<position[2]<62 and climb[4]==1
    return dict(result='PASS',frames=frames,crossing=crossing,health=health,position=position,climb=climb,
        scope=('Uninterrupted Area2 prefix and seven L2S3 kills; cross the35m walkways and doors to engage the second shaft ladder. Upper climb and exit remain open.' if crossing else 'Uninterrupted Area2 prefix and seven L2S3 kills; diagnostic climb to the50m platform. Intended onward route crosses at35m instead.'))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--crossing',action='store_true');args=parser.parse_args()
    folder=root/('artifacts/area3-shaft-crossing' if args.crossing else 'artifacts/area3-main-shaft');folder.mkdir(parents=True,exist_ok=True)
    source=folder/'input.bin';source.write_bytes(build_input(args.crossing))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(folder/'run.log').write_text(log);run.check_returncode()
    report=verify(log,args.crossing);(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)

if __name__=='__main__':main()
