"""Uninterrupted Area2 route, shotgun encounter and first L2S3 ladder.
Run replay_area3_hall.py first. No placement, forced exits or inventory grants.
"""
import argparse,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def build_input(landing=False):
    prefix=(root/'artifacts/area3-hall-replay/input.bin').read_bytes()
    assert prefix[:8]==b'RFI6'+struct.pack('<I',48) and len(prefix)==8+9600*48
    records=bytearray(prefix)
    # Enemy drops now grant a Riot Stick before this encounter; cycle past it.
    for i in range(9600,9950):
        x,z=(-.995,.101) if i<9620 else ((.101,.995) if 9640<=i<9720 else (0,0))
        records.extend(struct.pack('<5f7I',x,0,z,0,-.98 if 9730<=i<9750 else 0,0,0,1,0,0,int(i in (9752,9754)),int(9760<=i<9790)))
    for i in range(9950,10400):
        x,z=(-.909,.417) if i<9961 else ((.417,.909) if 9980<=i<10066 else (0,0))
        records.extend(struct.pack('<5f7I',x,float(10080<=i<10270),z,0,0,0,0,1,0,0,0,0))
    for i in range(10400,10800):
        x,z=(.41483,.9099) if i<10407 else ((-.9099,.41483) if 10425<=i<10439 else (0,0))
        records.extend(struct.pack('<5f7I',x,float(10460<=i<10650),z,0,0,0,int(i==10460),1,0,0,0,0))
    if landing:
        for i in range(10800,10950):
            x,z=(.9099,-.41483) if i<10808 else ((.41483,.9099) if 10825<=i<10855 else (0,0))
            records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,1,0,0,0,0))
    return records

def verify(log,landing=False):
    def words(label):return list(map(int,next(l.split()[1:] for l in log.splitlines() if l.startswith(label+' '))))
    frames=10950 if landing else 10800
    assert f'Completed {frames} frames' in log and words('PLAYER_LIFE')[0]==0
    assert [l.split()[1:] for l in log.splitlines() if l.startswith('LEVEL_TRANSITION ')]==[['L2S2a.rfl','L2S3.rfl','5150','7275']]
    assert words('COMBAT')[:3]==[22,18,4] and words('SHOTGUN')[:5]==[3,12,5,1,3]
    assert words('PLAYER_AMMO')[:3]==[5,0,5]
    health=struct.unpack('<f',struct.pack('<I',words('ENEMY_COMBAT')[5]))[0];assert health==5
    rows={int(l.split()[1]):l.split()[1:] for l in log.splitlines() if l.startswith('NPC_COMBAT_ROW ')}
    assert all(float(rows[u][-1])<=0 for u in [2020,2047,1751,2114]) and float(rows[2061][-1])==100
    position=list(map(float,next(l.split()[1:] for l in log.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
    climb=words('PLAYER_CLIMB');assert climb[1]>0
    if landing:assert 3.2<position[1]<3.6 and 98<position[0]<99 and 65<position[2]<66 and climb[4]==1
    else:assert 4.5<position[1]<5 and 100<position[0]<101 and 66<position[2]<67 and climb[4]==2
    return dict(result='PASS',frames=frames,health=health,position=position,climb=climb,scope='Natural Area2 prefix, four L2S3 kills and shotgun pickup/use; jump-assisted body contact climbs first ladder. Main shaft and exit remain open.')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--landing',action='store_true');args=parser.parse_args()
    folder=root/('artifacts/area3-shaft-landing' if args.landing else 'artifacts/area3-shaft-replay');folder.mkdir(parents=True,exist_ok=True)
    source=folder/'input.bin';source.write_bytes(build_input(args.landing))
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')};env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
    run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(folder/'run.log').write_text(log);run.check_returncode()
    report=verify(log,args.landing);(folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
