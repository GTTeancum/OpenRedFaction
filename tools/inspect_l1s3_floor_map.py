"""Coarse static floor connectivity for route reconnaissance, not navigation proof."""
import collections,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/l1s3-floor-map';out.mkdir(exist_ok=True)
points=[(x,z) for x in range(-100,111,2) for z in range(-80,81,2)]
queries=''.join(f'{x} 45 {z} 0 -100 0 1120\n' for x,z in points)
r=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world-ray-at',str(root/'Installed_Game/levels1.vpp'),'L1S3.rfl'],input=queries,capture_output=True,text=True,check=True)
assert len(r.stdout.splitlines())==len(points)
rows=[];walk={};errors=[]
for (x,z),line in zip(points,r.stdout.splitlines()):
    f=line.split();assert len(f)==13
    if int(f[0]):errors.append(dict(x=x,z=z,status=int(f[0])));continue
    hit=int(f[1]);y=float(f[6]);normal_y=float(f[9]);rows.append([x,z,hit,y,normal_y,int(f[2])])
    if hit and 20<y<40 and normal_y>=.6:walk[(x,z)]=y
start=min(walk,key=lambda p:(p[0]-73)**2+(p[1]+67)**2)
queue=collections.deque([start]);reachable={start}
while queue:
    p=queue.popleft()
    for dx,dz in ((2,0),(-2,0),(0,2),(0,-2)):
        n=(p[0]+dx,p[1]+dz)
        if n not in reachable and n in walk and abs(walk[n]-walk[p])<=2:reachable.add(n);queue.append(n)
report=dict(status='OBSERVED',scope='Two-unit grid; first downward static ray, height20..40, normalY>=0.6, adjacent height difference<=2. Heuristic thresholds, no player clearance, headroom, dynamic doors, original navigation or route correctness. Narrow passages and multiple floor layers can disconnect this graph.',start=start,walkable=len(walk),reachable=sorted(reachable),errors=errors,rows=rows)
(out/'map.json').write_text(json.dumps(report,indent=2)+'\n');print(dict(status=report['status'],queries=len(points),walkable=len(walk),reachable=len(reachable),errors=len(errors)))
