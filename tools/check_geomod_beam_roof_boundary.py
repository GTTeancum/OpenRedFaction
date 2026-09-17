"""Verify beam95/roof80/air85 interface against installed compiled geometry."""
import json
from pathlib import Path
from inspect_geomod_source_topology import load
ROOT=Path(__file__).resolve().parents[1]
faces,meta=load('ctf06.rfl')
beam=[f for f in faces if f['source_word']==556]
roof=[f for f in faces if f['source_word']==478]
assert [f['id'] for f in beam]==[168] and [f['id'] for f in roof]==[164,165,166,167]
assert all(abs(p[1]-2.5)<1e-6 for f in beam+roof for p in f['points'])
def area(face):
 p=face['points'];return abs(sum(a[0]*b[2]-a[2]*b[0] for a,b in zip(p,p[1:]+p[:1])))*.5
def contains(face,x,z):
 p=face['points'];cross=[(b[0]-a[0])*(z-a[2])-(b[2]-a[2])*(x-a[0]) for a,b in zip(p,p[1:]+p[:1])]
 return all(c>=-1e-7 for c in cross) or all(c<=1e-7 for c in cross)
beam_area=sum(map(area,beam));roof_area=sum(map(area,roof))
assert abs(beam_area-3)<1e-5 and abs(roof_area-7)<1e-5
checks=0
# Cell centers avoid authored and compiled edge ambiguities. The two masks
# independently check exposed beam top and exposed roof underside.
for i in range(80):
 x=-8+(i+.5)*4/80
 for j in range(160):
  z=-4+(j+.5)*8/160;on_beam=-5.25<x<-4.75;in_air=-3<z<3
  assert sum(contains(f,x,z) for f in beam)==int(on_beam and in_air),(x,z,'beam')
  assert sum(contains(f,x,z) for f in roof)==int(not on_beam and not in_air),(x,z,'roof')
  checks+=2
report=dict(result='PASS',geometry_sha256=meta['geometry_sha256'],point_checks=checks,
 beam_faces=[f['id'] for f in beam],roof_faces=[f['id'] for f in roof],
 compiled_beam_top_area=beam_area,compiled_roof_bottom_area=roof_area,
 beam_top_total_area=4,beam_roof_contact_area=1,
 scope='Compiled boundary coverage at Y2.5; no general ordered CSG, dynamic support or live beam destruction acceptance')
out=ROOT/'artifacts/geomod-beam-roof-boundary.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
