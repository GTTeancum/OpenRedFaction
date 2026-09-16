"""Prepare ordinary two-rocket DEV recording; no process launch or synthetic debris.

First shot follows the verified blast recording; second shot waits through the
launcher cooldown. Final telemetry must prove an existing settled fragment resumes.
"""
from pathlib import Path
import argparse,struct,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--second-frame',type=int,default=450);p.add_argument('--yaw',type=float,default=.08);a=p.parse_args();assert a.second_frame>=350
 frames=a.second_frame+100;rows=[]
 for i in range(frames):
  yaw=a.yaw if a.second_frame-16<=i<a.second_frame-6 else 0
  rows.append(struct.pack('<5f7I',0,0,.8 if 130<=i<306 else 0,yaw,.7 if i<90 else 0,0,0,0,int(i in (316,a.second_frame)),0,int(i in (10,20,30,40)),0))
 data=b'RFI6'+struct.pack('<I',48)+b''.join(rows);folder=ROOT/'artifacts/debris-relaunch';folder.mkdir(parents=True,exist_ok=True)
 (folder/'inputs.bin').write_bytes(data);(folder/'recipe.json').write_text(json.dumps(dict(frames=frames,level='glass_house.rfl',dev_room=True,combat_trace=True,shots=[316,a.second_frame],second_yaw=a.yaw,sha256=hashlib.sha256(data).hexdigest(),acceptance='DEBRIS_RELAUNCH final cumulative settled-resumed field>0, >=2 passes; inspect preserved age/state and pre-existing slot. Mere spawned count or new moving debris is insufficient.',status='candidate; live replay not yet run'),indent=2)+'\n')
 print(folder/'inputs.bin')
if __name__=='__main__':main()
