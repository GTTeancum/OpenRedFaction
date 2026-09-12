"""Independent resource accounting and archive-backed query equivalence."""
import json,math,re,struct,subprocess,sys
from pathlib import Path
from inspect_clutter_records import sections,inspect
from inspect_models import inspect as inspect_model
root=Path(__file__).resolve().parents[1];inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
def read(name):
 a=next(a for a in inv if a['path']=='meshes.vpp');e=next(e for e in a['vpp']['entries'] if e['name'].lower()==name.lower())
 with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
def hash_bytes(h,data):
 for b in data:h=((h^b)*16777619)&0xffffffff
 return h
f32=lambda f:struct.unpack('<f',struct.pack('<f',f))[0]
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
appearances=json.loads((root/'artifacts/clutter-scene-skins.json').read_text())['appearances']
classes={a['class_name'].lower():a['model'] for a in appearances}
rows=next(inspect(data) for level,data in sections() if level['file'].lower()=='l1s1.rfl')
bounds={r['model'].lower():r['sphere'] for r in json.loads((root/'artifacts/model-static-bounds.json').read_text())['records']}
models={};cases=[];resource_hash=2166136261;resource_bytes=parts=lods=0
for index,row in enumerate(rows):
 name=classes.get(row['class_name'].decode('cp1252').lower())
 if name is None:continue
 key=name.lower()
 if key not in models:
  raw=read(name);sections_model=[s for s in inspect_model(raw)['sections'] if s['type']=='0x5355424d'];model_lods=[];first=0
  resource_bytes+=20+44*len(sections_model);parts+=len(sections_model)
  for s in sections_model:
   n=len(s['lods']);start=s['offset']+64+n*4
   resource_hash=hash_bytes(resource_hash,raw[start:start+12]+raw[start+16:start+40]+w(first+n-1,first))
   model_lods.extend(s['lods']);first+=n
  lods+=len(model_lods)
  for lod in model_lods:
   resource_bytes+=20+lod['data_bytes']+20*lod['batches']
   resource_hash=hash_bytes(resource_hash,w(lod['flags'])+struct.pack('<H',lod['batches']))
   resource_hash=hash_bytes(resource_hash,raw[lod['data_offset']:lod['data_offset']+lod['data_bytes']])
  models[key]=dict(name=name,input=bytearray(),indices=[])
 center=bounds[key];r=f32(math.sqrt((center[0]*center[0]+center[1]*center[1])+center[2]*center[2])+center[3]);radius=f32(r+1)
 position=struct.unpack('<3f',row['position'])
 for axis in range(3):
  for side in range(2):
   start=list(position);delta=[0.,0.,0.];start[axis]=f32(start[axis]+(radius if side else -radius));delta[axis]=f32((-2 if side else 2)*radius)
   query=row['position']+row['matrix']+struct.pack('<6f',*start,*delta)+bytes(32)
   assert len(query)==104
   models[key]['input']+=query+bytes(32)+w(1);models[key]['indices'].append(len(cases));cases.append([row['uid'],None])
for model in models.values():
 output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(root/'Installed_Game/meshes.vpp'),model['name'],'--collision-trace'],input=bytes(model['input']))
 assert len(output)==len(model['indices'])*140
 for i,index in enumerate(model['indices']):cases[index][1]=output[i*140:(i+1)*140]
query_hash=2166136261;hits=0
for uid,result in cases:
 hits+=bool(struct.unpack_from('<I',result)[0]);query_hash=hash_bytes(query_hash,w(uid)+result[4:]+result[:4])
expected=[len(models),parts,lods,resource_bytes,len(cases),hits,resource_hash,query_hash,0]
log=Path(sys.argv[1]) if len(sys.argv)>1 else root/'artifacts/clutter-collision-pc.log'
actual=list(map(int,next(l for l in log.read_text().splitlines() if l.startswith('CLUTTER_COLLISION ')).split()[1:]))
assert actual==expected,(actual,expected)
report=dict(result='PASS',expected=expected,scope='L1S1 serialized model bytes independently derive collision part/LOD selection and raw data hash/budget. Six world-axis rays per authored placement are generated independently from verified original model bounds and compared through the archive-backed trace probe to registered scene query/scratch/hit hash. Shared low-level trace code; no new original-oracle or live collision scheduling claim.')
(root/'artifacts/clutter-scene-collision.json').write_text(json.dumps(report,indent=2));print(report)
