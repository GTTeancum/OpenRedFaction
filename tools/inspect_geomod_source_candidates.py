"""Census authored brush neighborhoods; no scene admission or asset mutation."""
import collections, hashlib, json, struct
from pathlib import Path
from inspect_geomod_source_topology import load, topology, convex
ROOT=Path(__file__).resolve().parents[1]
brush_file=ROOT/'artifacts/future-vehicles-re/ctf06-editor-brushes.json'
brushes=json.loads(brush_file.read_text(encoding='utf-8'))
faces,meta=load('ctf06.rfl')
f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
links=collections.defaultdict(list)
for face in faces:links[face['source_word']].append(face)
for b in brushes:
 basis=b['basis'][3:9]+b['basis'][:3]
 def world(p):return tuple(f32(f32(sum(p[j]*basis[j*3+k] for j in range(3)))+b['position'][k]) for k in range(3))
 b['world_faces']=[dict(f,points=[world(p) for p in f['points']]) for f in b['faces']]
 points=[p for f in b['world_faces'] for p in f['points']]
 b['bounds']=[[min(p[k] for p in points),max(p[k] for p in points)] for k in range(3)]
 b['compiled']=[f for source in b['faces'] for f in links[source['source_word']]]
def overlap(a,b):return all(x[0]<=y[1]+1e-5 and x[1]>=y[0]-1e-5 for x,y in zip(a['bounds'],b['bounds']))
rows=[]
for b in brushes:
 if b['tail'][2] or not 4<=len(b['faces'])<=32 or not b['compiled']:continue
 rooms=sorted(set(f['room'] for f in b['compiled']))
 if rooms!=[3]:continue
 near=[other for other in brushes if other['uid']!=b['uid'] and overlap(b,other)]
 nonzero=[n for n in near if n['tail'][2]!=0];solid=[n for n in near if n['tail'][2]==0]
 def neighbor(n):
  depth=[min(x[1],y[1])-max(x[0],y[0]) for x,y in zip(b['bounds'],n['bounds'])]
  return dict(uid=n['uid'],index=n['index'],flags=n['tail'][2],faces=len(n['faces']),bounds=n['bounds'],aabb_depth=depth,contact_only=any(d<=1e-5 for d in depth))
 t=topology(b['world_faces']);c=convex(b['world_faces'],False)
 rows.append(dict(uid=b['uid'],index=b['index'],bounds=b['bounds'],faces=len(b['faces']),compiled_faces=len(b['compiled']),
  textures=b['textures'],closed=t['closed_oriented'],convex=c['convex'],
  nonzero_brushes=[neighbor(n) for n in nonzero],
  solid=[neighbor(n) for n in solid],
  current_profile=b['uid'] in [93,94,96,97]))
report=dict(scope='Exact authored face ownership and transformed AABB overlap census; not general ordered CSG, support or gameplay admission',geometry_sha256=meta['geometry_sha256'],brush_census_sha256=hashlib.sha256(brush_file.read_bytes()).hexdigest(),rows=rows)
out=ROOT/'artifacts/geomod-source-candidates.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
for r in sorted(rows,key=lambda r:len(r['solid'])+len(r['nonzero_brushes'])):
 if r['closed'] and r['convex'] and len(r['solid'])<=5:
  print(r['uid'],'bounds',r['bounds'],'solids',[n['uid'] for n in r['solid']],'nonzero',[(n['uid'],n['flags'],n['contact_only']) for n in r['nonzero_brushes']],'textures',r['textures'])
print('candidate rows',len(rows),'report',out)
