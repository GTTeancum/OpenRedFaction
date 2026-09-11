"""Authored footstep declaration parsing and transactional guards."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
text='\n'.join(line.split('//',1)[0] for line in raw.decode('cp1252').splitlines())
pattern=r'\+State:\s*"([^"\r\n]*)"\s*"([^"\r\n]*)"\s*\+Footstep\s+Trigger:\s*([^\s]+)\s+([^\s]+)'
rows=list(re.finditer(pattern,text));assert rows
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
def run(text,state='walk',weapon=''):
 return subprocess.check_output([probe,'--state-text','fixture',weapon,state],input=text.encode())
for row in rows:
 name,motion,left,right=row.groups()
 snippet='$Name: "fixture"\n'+row[0]
 result=run(snippet,name)
 assert result==struct.pack('<i',0)+motion.encode().ljust(64,b'\0')+struct.pack('<Iff',2,float(left),float(right)),row[0]
valid='$Name: "fixture"\n+State: "walk" "walk.mvf"'
assert run(valid)==struct.pack('<i',0)+b'walk.mvf'.ljust(64,b'\0')+bytes(12)
guards=[(valid+'\n+Footstep Trigger: 5',''),(valid+'\n+Footstep Trigger: nope 19',''),
 (valid+'\n+Footstep Trigger: 5 19\n+Footstep Trigger: 6 20',''),
 (valid+'\n+State: "walk" "other.mvf"',''),(valid,'missing'),
 (valid+'\n+Footstep Trigger: 1e999 2','')]
for snippet,weapon in guards:
 result=run(snippet,weapon=weapon);assert struct.unpack('<i',result[:4])[0]!=0 and result[4:]==b'\xa5'*76
report=dict(result='PASS',authored_pairs=len(rows),guards=len(guards),scope='Every installed footstep pair tested as its original declaration snippet against independent numeric parsing; full selected-class integration covered separately by three-level catalog checks. Full original parser execution excluded.')
(root/'artifacts/state-markers.json').write_text(json.dumps(report,indent=2));print(report)
