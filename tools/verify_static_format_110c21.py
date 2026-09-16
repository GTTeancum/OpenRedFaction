"""Original static vertex loop on actual LavaTester01 first-batch data.
Proves CPU stream consumption, not alternate graphics-backend packed formats.
"""
import sys,struct,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'));sys.path.insert(0,str(ROOT/'tools'))
from inspect_models import inspect
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
a=next(a for a in inventory['files'] if a['path']=='meshes.vpp');e=next(e for e in a['vpp']['entries'] if e['name'].lower()=='lavatester01.v3m')
with (ROOT/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
lod=next(s['lods'][0] for s in inspect(data)['sections'] if s.get('lods'));start=lod['data_offset'];v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',data,start+lod['data_bytes']+4)
assert fmt==0x110c21 and v==16
relative=(lod['batches']*56+15)&~15;regions=[]
for size in [p,p,uv,ix,t*16,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]:regions.append(start+relative);relative=(relative+size+15)&~15
actual=[data[regions[0]+i*12:regions[0]+i*12+12]+data[regions[1]+i*12:regions[1]+i*12+12]+data[regions[2]+i*8:regions[2]+i*8+8]+data[regions[6]+i*8:regions[6]+i*8+8] for i in range(8)]
distances=[struct.unpack_from('<h',data,regions[5]+i*2)[0] for i in range(8)]
source=(ROOT/'tools/verify_model_static_render_batch.py').read_text().split("probe=str(root/")[0]
source=source.replace('for n in range(512):','for descriptor in (0x110c21,0x518c41):\n    n=0; rng.seed(123)')
source=source.replace('vertices.append(v)','v=actual[i];distance=actual_distances[i];distances[-1]=distance;vertices.append(v)')
source=source.replace('u.mem_write(batch,bytes(56));','u.mem_write(batch,bytes(56));put(batch,descriptor);')
ns=dict(__file__=str(ROOT/'tools/verify_model_static_render_batch.py'),actual=actual,actual_distances=distances)
exec(source,ns)
assert len(ns['expected'])==2 and ns['expected'][0]==ns['expected'][1]
report=dict(result='PASS',model='LavaTester01.v3m',batch_format=hex(fmt),vertices=8,original_range='52e11b..52e438',output_bytes=len(ns['expected'][0]),scope='Actual serialized float position/normal/UV and signed16reuse through original static CPU vertex loop; descriptor change leaves output bit-identical. Triangle/index raw audit separate. Does not claim full renderer backend dispatch or material parity.')
(ROOT/'artifacts/crater-shading-re/static-format-110c21.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
