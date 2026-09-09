"""Inventory v180 mover record boundaries using original loader evidence."""
import json,struct
from pathlib import Path
from inspect_geometry import inspect
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
for level in levels:
 section=next((s for s in level['sections'] if s['type']=='0x2000'),None)
 if section is None:continue
 archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 count,=struct.unpack_from('<I',data);cursor=4;records=[]
 for index in range(count):
  begin=cursor;uid,*pose=struct.unpack_from('<i12f',data,cursor);cursor+=52;geometry_offset=cursor
  geometry=inspect(data[cursor:], allow_unowned=True);geometry_bytes=len(data)-cursor-geometry['tail_bytes'];cursor+=geometry_bytes
  legacy=geometry['tail_word'];cursor+=legacy*12
  trailer=struct.unpack_from('<3I',data,cursor);cursor+=12
  assert cursor<=len(data),(level['file'],index,cursor,len(data))
  records.append(dict(uid=uid,offset=begin,bytes=cursor-begin,position=pose[:3],orientation_disk=pose[3:],geometry_offset=geometry_offset,geometry_bytes=geometry_bytes+legacy*12,textures=geometry['textures'],rooms=geometry['rooms'],vertices=geometry['vertices'],faces=geometry['faces'],mappings=geometry['mappings'],legacy_records=legacy,trailer=trailer))
 assert cursor==len(data),(level['file'],cursor,len(data))
 results.append(dict(file=level['file'],archive=level['archive'],count=count,bytes=len(data),records=records))
report=dict(result='PASS',levels=len(results),movers=sum(x['count'] for x in results),faces=sum(r['faces'] for l in results for r in l['records']),scope='Bounded Python inventory of v180 section 2000 using 463c60 loader sequence and existing geometry layout. Exact section exhaustion; not a C reader, original parser differential or runtime mover construction.',results=results)
(root/'artifacts/movers.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
