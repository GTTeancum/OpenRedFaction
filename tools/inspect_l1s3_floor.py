"""Targeted static-world rays around the observed L1S3 route departure."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
points=[(52.463,-34.681),(50.81,-32.23),(50.27,-29.7),(50.46,-26.74),(48,-25),(52,-25),(50,-20),(35.826,-10.562),(27.03,-22.47)]
queries=''.join(f'{x} 40 {z} 0 -100 0 1120\n' for x,z in points)
r=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world-ray-at',str(root/'Installed_Game/levels1.vpp'),'L1S3.rfl'],input=queries,capture_output=True,text=True,check=True)
rows=[]
assert len(r.stdout.splitlines())==len(points)
for point,line in zip(points,r.stdout.splitlines()):
    fields=line.split();assert len(fields)==13 and int(fields[0])==0
    rows.append(dict(xz=point,matched=int(fields[1]),face=int(fields[2]),room=int(fields[3]),fraction=float(fields[4]),point=list(map(float,fields[5:8])),normal=list(map(float,fields[8:11])),face_flags=int(fields[11]),portal=int(fields[12])))
groups=next(x for x in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if x['file']=='L1S3.rfl')
report=dict(status='OBSERVED',rays=rows,moving_groups=[dict(name=g['name'],members=g['ids2'],keys=[k['position'] for k in g['keys']]) for g in groups['records']],scope='Shared static-world ray query from Y40 downward100 with flags0x460, initial geometry state. Not a player sphere sweep, intended-route proof, dynamic collision or original-game trace. The known lift is spatially separate; rays show low surfaces near the departure and upper surfaces farther ahead.')
(root/'artifacts/l1s3-floor-observation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
