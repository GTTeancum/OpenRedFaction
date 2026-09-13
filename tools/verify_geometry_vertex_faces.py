"""Check retained adjacency owner against authored geometry membership/order."""
import json,struct,subprocess
from pathlib import Path
from inspect_geometry import inspect
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());reports=[]
probe=root/'build/pc/Release/rf_geometry_probe.exe'
for level in levels:
 section=next((s for s in level['sections'] if s['type']=='0x100'),None)
 if not section:continue
 archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file']);path=root/'Installed_Game'/level['archive']
 with path.open('rb') as stream:stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
 g=inspect(data);adj=[[] for _ in range(g['vertices'])];at=g['vertices_offset']+g['vertices']*12+4
 for face in range(g['faces']):
  mapping=struct.unpack_from('<I',data,at+20)[0];count=struct.unpack_from('<I',data,at+52)[0];at+=56;stride=12 if mapping==0xffffffff else 20;seen=set()
  for j in range(count):
   vertex=struct.unpack_from('<I',data,at)[0];at+=stride
   if vertex not in seen:adj[vertex].append(face);seen.add(vertex)
 args=[str(probe),str(path),level['file'],'--adjacency']
 lines=subprocess.check_output(args+['262144'],text=True).splitlines();assert lines[0]=='0',level['file']
 vertices,links,resident=map(int,lines[1].split());assert vertices==len(adj) and links==sum(map(len,adj));assert len(lines)==vertices+2
 for expected,line in zip(adj,lines[2:]):
  values=list(map(int,line.split()));assert values==[len(expected),*expected],level['file']
 failure=subprocess.check_output(args+[str(resident-1)],text=True).splitlines();assert len(failure)==1 and int(failure[0])!=0
 reports.append(dict(file=level['file'],vertices=vertices,links=links,pc_resident_bytes=resident))
report=dict(result='PASS',levels=len(reports),vertices=sum(r['vertices'] for r in reports),links=sum(r['links'] for r in reports),records=reports,scope='Actual PC archive/level/geometry loading and bounded adjacency owner for all serialized faces; every ordered list, one-byte-under budget failure and idempotent close checked. Original loader rejection/removal, filtered membership and native execution excluded.')
(root/'artifacts/geometry-vertex-faces.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'});print(next(r for r in reports if r['file']=='L1S1.rfl'))
