"""Continue the uninterrupted Area2 route into the L2S3 main shaft.
Normal replay inputs only: no staged pose, health, inventory or forced exits.
"""
import argparse,json,math,os,struct,subprocess
from pathlib import Path
from replay_area3_shaft import build_input as landing_input
root=Path(__file__).resolve().parents[1]

def build_input(crossing=False,second_climb=False,exit_landing=False,exit_combat=False):
    exit_landing=exit_landing or exit_combat
    second_climb=second_climb or exit_landing
    crossing=crossing or second_climb
    frames=12910 if exit_combat else 12620 if exit_landing else 12600 if second_climb else 12110 if crossing else 11950
    data=bytearray(landing_input(True))
    for i in range(10950,frames):
        if exit_combat and i>=12620:
            yaw=1.799+.98/60*(min(82,max(0,i-12620))+min(6,max(0,i-12703)))
            c,s=math.cos(yaw),math.sin(yaw)
            wx,wz=(-.6,-.8) if 12711<=i<12727 else ((0,-.55) if 12727<=i<12765 else (0,0))
            pitch=-.98 if i<12636 else (.98 if 12755<=i<12765 else 0)
            value=(c*wx-s*wz,0,s*wx+c*wz,pitch,.98 if i<12702 or 12703<=i<12709 else 0,0,int(i==12711),1,0,0,0,int(12711<=i<12860))
        elif exit_landing and i>=12443:
            x,z=(.42,.25) if 12460<=i<12530 else (0,0)
            value=(x,0,z,0,0,0,0,1,0,0,0,0)
        elif second_climb and i>=12110:
            value=(0,float(i<12550),0,0,0,0,0,1,0,0,0,0)
        elif crossing and i>=11595:
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

def verify(log,crossing=False,second_climb=False,exit_landing=False,exit_combat=False):
    exit_landing=exit_landing or exit_combat
    second_climb=second_climb or exit_landing
    crossing=crossing or second_climb
    frames=12910 if exit_combat else 12620 if exit_landing else 12600 if second_climb else 12110 if crossing else 11950
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    assert f'Completed {frames} frames' in log and words('PLAYER_LIFE')[0]==0
    assert [l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
    assert words('COMBAT')[:3]==([59,28,9] if exit_combat else [34,24,7]) and words('RIFLE_ALT')[:3]==([37,10,5] if exit_combat else [12,6,3])
    assert words('PLAYER_AMMO')[:3]==[8,0,5 if exit_combat else 30] and words('SHOTGUN')[:5]==[3,12,5,1,3]
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==5
    rows={int(l.split()[1]):l.split()[1:] for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert all(float(rows[u][-1])<=0 for u in [2020,2047,1751,2114,1773,2067,1757])
    assert float(rows[2061][-1])==100
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    climb=words('PLAYER_CLIMB');assert climb[1]>0
    if exit_combat:
        assert 108<position[0]<108.6 and 67.8<position[1]<68 and 57<position[2]<57.4 and climb[4]==1
        assert all(float(rows[u][-1])<=0 for u in (2086,2087))
    elif exit_landing:
        assert 109.8<position[0]<110 and 67.8<position[1]<68 and 58.8<position[2]<59 and climb[4]==1
        assert all(float(rows[u][-1])==100 for u in (2086,2087))
    elif second_climb:
        assert 110<position[0]<111 and 73<position[1]<73.5 and 61<position[2]<62 and climb[3:5]==[3,2]
        assert all(float(rows[u][-1])==100 for u in (2086,2087))
    elif crossing:assert 110<position[0]<111 and 35.7<position[1]<36.2 and 60.5<position[2]<61.5 and climb[3:5]==[3,2]
    else:assert 97<position[0]<98 and 49.8<position[1]<50.5 and 61<position[2]<62 and climb[4]==1
    return dict(result='PASS',frames=frames,crossing=crossing,second_climb=second_climb,exit_landing=exit_landing,exit_combat=exit_combat,health=health,position=position,climb=climb,
        scope=('Uninterrupted campaign clears nine L2S3 enemies and reaches the exit corridor alive; onward transition remains open.' if exit_combat else 'Uninterrupted campaign reaches a stable exit landing; corridor entry and guards remain open.' if exit_landing else 'Uninterrupted campaign reaches the second ladder top alive; exit landing and guards remain open.' if second_climb else 'Uninterrupted Area2 prefix and seven L2S3 kills; cross the35m walkways and doors to engage the second shaft ladder. Upper climb and exit remain open.' if crossing else 'Uninterrupted Area2 prefix and seven L2S3 kills; diagnostic climb to the50m platform. Intended onward route crosses at35m instead.'))

def build_exit_walk():
    data=bytearray(build_input(exit_combat=True))
    for i in range(12910,13200):
        data.extend(struct.pack('<5f7I',0,0,float(12925<=i<13120),0,-.98 if i<12916 else 0,0,0,1,0,0,0,0))
    return data

def verify_exit_walk(log):
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    assert 'Completed 13200 frames' in log and words('PLAYER_LIFE')[0]==0
    transitions=[l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]
    assert transitions==[['L2S2a.rfl','L2S3.rfl','5150','7275'],['L2S3.rfl','L3S1.rfl','6604','13085']]
    assert words('PLAYER_AMMO')[:3]==[8,0,5]
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==5
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    assert -58.8<position[0]<-58.5 and -3.3<position[1]<-3 and -21.1<position[2]<-20.7
    return dict(result='PASS',frames=13200,health=health,position=position,transitions=transitions,
        scope='Uninterrupted normal-input route reaches L3S1 through authored exit6604. Combat counters reset on transition; the identical exit-combat prefix separately verifies nine L2S3 kills.')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    route=parser.add_mutually_exclusive_group()
    route.add_argument('--crossing',action='store_true')
    route.add_argument('--second-climb',action='store_true')
    route.add_argument('--exit-landing',action='store_true')
    route.add_argument('--exit-combat',action='store_true')
    route.add_argument('--exit-walk',action='store_true')
    args=parser.parse_args()
    folder=root/('artifacts/area3-exit-walk' if args.exit_walk else 'artifacts/area3-exit-combat' if args.exit_combat else 'artifacts/area3-exit-landing' if args.exit_landing else 'artifacts/area3-second-climb' if args.second_climb else 'artifacts/area3-shaft-crossing' if args.crossing else 'artifacts/area3-main-shaft');folder.mkdir(parents=True,exist_ok=True)
    source=folder/'input.bin';source.write_bytes(build_exit_walk() if args.exit_walk else build_input(args.crossing,args.second_climb,args.exit_landing,args.exit_combat))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(folder/'run.log').write_text(log);run.check_returncode()
    report=verify_exit_walk(log) if args.exit_walk else verify(log,args.crossing,args.second_climb,args.exit_landing,args.exit_combat);(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)

if __name__=='__main__':main()
