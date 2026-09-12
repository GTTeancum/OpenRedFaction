"""Independent opening attachment residency/hash versus campaign telemetry."""
import json,struct,sys
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
a=next(a for a in inv if a['path']=='meshes.vpp');entries={e['name'].lower():e for e in a['vpp']['entries']}
names=list(dict.fromkeys(a['model'].lower() for a in json.loads((root/'artifacts/clutter-scene-skins.json').read_text())['appearances']))
data=bytearray();count=0
for name in names:
 e=entries[name]
 with (root/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 lod=next(s for s in inspect(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
 count+=lod['props']
 for i in range(lod['props']):
  s=lod['attachment_offset']+100*i;data+=raw[s:s+68]+bytes(4)+raw[s+68:s+100]
h=2166136261
for b in data:h=((h^b)*16777619)&0xffffffff
expected=[len(names),count,12*len(names)+len(data),h]
log=(Path(sys.argv[1]) if len(sys.argv)>1 else root/'artifacts/clutter-tags-pc.log').read_text()
actual=list(map(int,next(l for l in log.splitlines() if l.startswith('CLUTTER_TAGS ')).split()[1:]));assert actual==expected,(actual,expected)
render=list(map(int,next(l for l in log.splitlines() if l.startswith('CLUTTER_RENDER ')).split()[1:]))
budget=[203444+170*12+len(data),212236+170*12+len(data)]
assert render[4:6]==budget,(render,budget)
r=dict(result='PASS',expected=expected,render_budget=budget,scope='First-submesh first-LOD serialized attachments in independently verified appearance/model order; decoded record hash and owner/scene budget. No live glare creation or parent pose claim.')
(root/'artifacts/clutter-scene-tags.json').write_text(json.dumps(r,indent=2));print(r)
