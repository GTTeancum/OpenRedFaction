"""Generate process-local APC aiming, primary and ballistic destruction replay."""
import json
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'artifacts/apc-combat-replay/inputs.bin'
path.parent.mkdir(parents=True,exist_ok=True)
rows=[]
for frame in range(480):
    # RFI6: move3,look2,crouch,jump,use,fire,reload,cycle,alternate.
    forward=float(10<=frame<25 or 70<=frame<80)
    pitch=-.2 if 80<=frame<100 else 0
    rows.append(struct.pack('<5f7I',0,0,forward,pitch,0,0,0,int(frame==40),
        int(120<=frame<150),0,0,int(180<=frame<381)))
path.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
manifest={'status':'PREPARED_NOT_RUN','frames':480,'input':str(path),
    'env':{'RF_REPLAY_LEVEL':'ctf06.rfl','RF_REPLAY_ARCHIVE':'levelsm.vpp','RF_REPLAY_DEV_ROOM':'1','RF_REPLAY_VEHICLE':'apc'},
    'expected':['One entry; remains seated','Nonzero primary and secondary launches with finite ammo debit',
        'Ballistic contacts cause blast damage and accepted GeoMod cuts when terrain permits',
        'Actual cockpit, changed camera pitch and vehicle health/ammo HUD visible at endpoint'],
    'scope':'Bounded enemy-free DEV combat, not NPC damage or full campaign coverage'}
path.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(path)
