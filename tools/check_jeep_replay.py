"""Generate process-local Jeep driver/gunner inputs; no host input or launch."""
import argparse
import json
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--exit',action='store_true',help='Return to driver then exit; default ends at gunner station')
a=p.parse_args()
path=ROOT/'artifacts/jeep-replay'/('exit.bin' if a.exit else 'gunner.bin')
path.parent.mkdir(parents=True,exist_ok=True)
rows=[]
for frame in range(600 if a.exit else 460):
    # RFI6: move3,look2,crouch,jump,use,fire,reload,cycle,alternate.
    forward=float(10<=frame<62 or 100<=frame<150)
    pitch=-.2 if 340<=frame<360 else 0
    yaw=.3 if 340<=frame<360 else 0
    use=frame==80 or (a.exit and frame==540)
    switch=frame==320 or (a.exit and frame==490)
    # Early driver trigger must not fire; late gunner trigger must fire.
    primary=180<=frame<195 or 380<=frame<420
    rows.append(struct.pack('<5f7I',0,0,forward,pitch,yaw,0,0,int(use),int(primary),0,int(switch),0))
path.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
path.with_suffix('.json').write_text(json.dumps({'status':'PREPARED_NOT_RUN','frames':len(rows),'input':str(path),
    'env':{'RF_REPLAY_LEVEL':'ctf06.rfl','RF_REPLAY_ARCHIVE':'levelsm.vpp','RF_REPLAY_DEV_ROOM':'1','RF_REPLAY_VEHICLE':'jeep'},
    'expected':['One entry;driver drive;parked seat switch;gunner fire only','Source-aware finite gun ammo and world impacts',
        'Authored mounted gun and role HUD visible;exit variant restores on-foot control'],
    'scope':'Single player DEV driver/gunner roles;NPC driver and campaign sequence unimplemented'},indent=2)+'\n')
print(path)
