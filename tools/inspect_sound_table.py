"""Inventory authored sound defaults and controller overlaps; no runtime ordering claim."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(r for r in inventory['files'] if r['path'].lower()=='tables.vpp')
entry=next(r for r in archive['vpp']['entries'] if r['name'].lower()=='sounds.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as stream:
 stream.seek(entry['offset']);data=stream.read(entry['size'])
assert len(data)==entry['size']
rows=[];started=ended=False
for line in data.decode('cp1252').splitlines():
 line=line.split('//',1)[0].strip()
 if not line:continue
 if line=='#Sounds Start':assert not started;started=True;continue
 if line=='#Sounds End':assert started;ended=True;break
 assert started
 match=re.fullmatch(r'"([^"\n]+)"\s+(\S+)\s+(\S+)\s+(\S+)',line);assert match,line
 name,near,volume,rolloff=match.groups()
 rows.append(dict(index=len(rows),name=name,near=float(near),volume=float(volume),rolloff=float(rolloff)))
assert started and ended and len(rows)<=2048
by_name={r['name'].lower():r for r in rows};assert len(by_name)==len(rows)
levels=json.loads((root/'artifacts/moving-groups.json').read_text())['results'];overlaps=[]
for level in levels:
 for group in level['records']:
  for slot,sound in enumerate(group['sounds']):
   row=by_name.get(sound['name'].lower())
   if row is not None:overlaps.append(dict(level=level['file'],controller=group['name'],slot=slot,authored_volume=sound['value'],table=row))
report=dict(result='PASS',table_sha256=hashlib.sha256(data).hexdigest(),entries=len(rows),levels=len(levels),overlaps=overlaps,rows=rows,scope='Strict inventory of installed sounds.tbl and existing authored controller inventory. Not original parser execution, registration order or runtime metadata integration.')
(root/'artifacts/sound-table-inventory.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k!='rows'})
