"""Check retained class default weapons against the installed tables."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
names=json.loads((root/'artifacts/weapon-names.json').read_text())['names']
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
parts=re.split(r'\$Name:\s*"([^"\r\n]+)"',text);rows=[]
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe');tables=str(root/'Installed_Game/tables.vpp')
def resolve(name):return [n.lower() for n in names].index(name.lower()) if name.lower() in [n.lower() for n in names] and name else -1
for cls,body in zip(parts[1::2],parts[2::2]):
 authored=[re.findall(r'\$Default '+key+r':\s*"([^"\r\n]*)"',body) for key in ('Primary','Secondary')]
 assert all(len(a)==1 for a in authored)
 ids=[resolve(a[0]) for a in authored]
 got=subprocess.check_output([probe,'--default-weapons',tables,cls.upper()])
 assert got==struct.pack('<3i',0,*ids),(cls,got,ids)
 rows.append(dict(entity_class=cls,primary=ids[0],secondary=ids[1],authored=[a[0] for a in authored]))
# Empty and unknown are both original -1; duplicate/missing tags are port errors.
for primary,secondary in [('', ''),('missing weapon','Riot Stick'),('rIoT sTiCk','FIGHTER ROCKET')]:
 snippet=f'$Name: "fixture"\n$Default Primary: "{primary}"\n$Default Secondary: "{secondary}"'
 got=subprocess.check_output([probe,'--default-weapons-text',tables,'fixture'],input=snippet.encode())
 assert got==struct.pack('<3i',0,resolve(primary),resolve(secondary))
base='$Name: "fixture"\n$Default Primary: ""\n$Default Secondary: ""'
guards=[('$Name: "fixture"','fixture'),(base+'\n$Default Primary: "Riot Stick"','fixture'),
 ('$Name: "fixture"\n$Default Primary: unquoted\n$Default Secondary: ""','fixture'),(base,'missing')]
for snippet,cls in guards:
 got=subprocess.check_output([probe,'--default-weapons-text',tables,cls],input=snippet.encode())
 assert struct.unpack('<i',got[:4])[0]!=0 and got[4:]==b'\xa5'*8
assert len(rows)==63
report=dict(result='PASS',classes=len(rows),lookup_cases=3,guards=len(guards),rows=rows,scope='Independent full installed-table metadata comparison; original41bf57..41bfe8 inspected, existing4c81f0 name lookup previously CPU-verified. No full original parser or NPC selector equivalence claim.')
(root/'artifacts/entity-default-weapons.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})
