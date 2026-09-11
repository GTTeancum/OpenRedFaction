"""Check shared model LOD inventory against independently parsed archive bytes."""
import json,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
entries={e['name'].lower():(a['path'],e) for a in json.loads((root/'artifacts/inventory.json').read_text())['files'] for e in a.get('vpp',{}).get('entries',[])}
results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--skeletons',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level],text=True)
 rows={};models=set();owner=None
 for line in output.splitlines():
  f=line.split('\t')
  if f[0]=='RENDER_LOD':rows[f[1],int(f[2])]=tuple(map(int,f[3:]));models.add(f[1])
  elif line.startswith('RENDER_MODELS '):owner=list(map(int,line.split()[1:]))
 expected={}
 for name in models:
  archive,e=entries[name.lower()]
  with (game/archive).open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
  lods=[lod for s in inspect(data)['sections'] for lod in s.get('lods',[])]
  for i,lod in enumerate(lods):expected[name,i]=(lod['batches'],lod['vertices'],lod['triangles'])
 assert rows==expected and owner[0]==len(models)
 results.append(dict(level=level,models=owner[0],lods=len(rows),resident_bytes=owner[1],vertices=sum(r[1] for r in rows.values()),triangles=sum(r[2] for r in rows.values())))
report=dict(result='PASS',scope='All shared opening skeleton models and all LOD directories against independently parsed archive bytes; exact/one-byte-short owner budgets and repeatable close. No textures, per-actor prepared skinning or GPU submission.',results=results)
(root/'artifacts/npc-render-models.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
