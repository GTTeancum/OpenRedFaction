"""Fight after the full-spawn rescue, retreat behind the doorway, and reload.
Run replay_area2_spawn.py first to generate the validated 3000-frame prefix.
"""
import argparse,json,os,struct,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--second-guard',action='store_true')
parser.add_argument('--medical-crate',action='store_true',help='Continue through two guards to the nearby medical crate')
parser.add_argument('--crate-closed-control',action='store_true',help='Follow the medical route without using the crate')
args=parser.parse_args()
args.medical_crate=args.medical_crate or args.crate_closed_control
args.second_guard=args.second_guard or args.medical_crate
frames=4500 if args.medical_crate else (4000 if args.second_guard else 3600)
case='cover-combat-medical-replay' if args.medical_crate else ('cover-combat-two-replay' if args.second_guard else 'cover-combat-replay')
folder=root/'artifacts'/case
if args.crate_closed_control:folder=folder/'closed-control'
folder.mkdir(parents=True,exist_ok=True)
prefix=(root/'artifacts/area2-spawn-replay/input.bin').read_bytes()
assert len(prefix)==8+3000*48 and prefix[:8]==b'RFI6'+struct.pack('<I',48)
records=bytearray(prefix[8:])
for i in range(3000,3600):
    x,z=(.1066,.9943) if i<3050 else ((-.48279,-.875735) if 3340<=i<3390 else (0,0))
    pitch,yaw=(-.1,-.9925) if 3050<=i<3074 else (0,0)
    fire=int(3080<=i<3340 and (i-3080)%32==0)
    records.extend(struct.pack('<5f7I',x,0,z,pitch,yaw,0,0,0,fire,int(i==3420),0,0))
if args.second_guard:
    for i in range(3600,4000):
        x,z=(.48279,.875735) if i<3633 else ((-.887,-.461) if 3780<=i<3813 else (0,0))
        yaw=-.98 if 3635<=i<3671 else 0
        fire=int(3672<=i<3780 and (i-3672)%32==0)
        records.extend(struct.pack('<5f7I',x,0,z,0,yaw,0,0,0,fire,int(i==3840),0,0))
if args.medical_crate:
    for i in range(4000,4500):
        x,z=(.461,-.887) if i<4010 else ((-.887,-.461) if 4040<=i<4100 else (0,0))
        records.extend(struct.pack('<5f7I',x,0,z,0,0,0,0,int(4090<=i<4200 and not args.crate_closed_control),0,0,0,0))
source=folder/'input.bin';source.write_bytes(prefix[:8]+records)
env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
env.update(RF_REPLAY_LEVEL='L2S2a.rfl',RF_REPLAY_TRACE='1')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/'frame.ppm')],cwd=root,env=env,capture_output=True,text=True)
(folder/'run.log').write_text(run.stdout+run.stderr);run.check_returncode()
def words(label):return list(map(int,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith(label+' '))))
combat,ammo,enemy=words('COMBAT'),words('PLAYER_AMMO'),words('ENEMY_COMBAT')
uid=5677 if args.second_guard else 8490
guard=int(next(l.split()[2] for l in run.stdout.splitlines() if l.startswith(f'NPC_BACKLINK_ROW {uid} ')))
position=list(map(float,next(l.split()[1:] for l in run.stdout.splitlines() if l.startswith('CAMPAIGN_FINAL_POSITION '))))
assert f'Completed {frames} frames' in run.stdout
assert combat[:3]==([13,8,2] if args.second_guard else [9,4,1]) and combat[3]==guard
assert ammo[2:5]==([16,13,2] if args.second_guard else [16,9,1]) and words('PLAYER_LIFE')[0]==0
assert 0<struct.unpack('<f',struct.pack('<I',enemy[5]))[0]<100
if args.medical_crate:
    assert 25<position[0]<26 and 3.8<position[2]<4.6
    restored=struct.unpack('<f',struct.pack('<I',words('PICKUP_VITALS')[2]))[0]
    if args.crate_closed_control:
        assert 'TAKEN_PICKUP l2s2a.rfl 8553' not in run.stdout
        assert words('ROTATING_DOORS')[2]==8512 and restored==0
    else:
        assert 'TAKEN_PICKUP l2s2a.rfl 8553' in run.stdout
        assert words('ROTATING_DOORS')[2]==8552 and restored==25
        assert struct.unpack('<f',struct.pack('<I',enemy[5]))[0]>40
else:
    assert 24<position[0]<25 and 9.5<position[2]<10.7
snapshots=[list(map(float,l.split()[1:])) for l in run.stdout.splitlines() if l.startswith('NPC_COMBAT_ROW ')]
assert words('NPC_COMBAT_FRAME')==[frames-1] and snapshots
for dead_uid in ([8490,5677] if args.second_guard else [8490]):
    assert next(row[-1] for row in snapshots if int(row[0])==dead_uid)<=0
report=dict(result='PASS',crate_closed_control=args.crate_closed_control,frames=frames,npc_snapshot_frame=frames-1,npc_snapshots=snapshots,combat=combat,ammo=ammo,enemy=enemy,position=position,
    prefix_sha256=hashlib.sha256(prefix).hexdigest(),
    scope='Full-spawn rescue prefix, then movement, aiming, semiautomatic shots, retreat and reload. Kills guard8490 (and5677 with --second-guard) and survives under cover; --medical-crate opens lid8552 and collects kit8553, unless --crate-closed-control omits Use and verifies no collection. Does not clear the remaining corridor or section exit.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='npc_snapshots'})
