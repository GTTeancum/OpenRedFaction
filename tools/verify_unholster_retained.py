"""Retained unholster scalars after archive closure and budget checks."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
files=json.loads((root/'artifacts/inventory.json').read_text())['files']
entry=next(e for a in files if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
blocks=re.split(rb'(?m)^\s*\$Name:\s*"([^"]+)"',raw);values={}
for i in range(1,len(blocks),2):
 declarations=re.findall(rb'(?m)^\s*\$Unholster\s+Delay:\s*([^\s/]+)',blocks[i+1]);assert len(declarations)<=1
 values[blocks[i].decode('cp1252').lower()]=struct.unpack('<I',struct.pack('<f',float(declarations[0]) if declarations else 0))[0]
levels=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 text=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--seeds',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),level],text=True)
 rows=[line.split('\t') for line in text.splitlines() if line.startswith('SEED_UNHOLSTER\t')];assert rows
 digest=2166136261
 for _,name,bits in rows:
  assert int(bits)==values[name.lower()],(level,name,bits)
  for byte in struct.pack('<I',int(bits)):digest=((digest^byte)*16777619)&0xffffffff
 levels.append(dict(level=level,classes=len(rows),nonzero=sum(int(row[2])!=0 for row in rows),added_bytes=4*len(rows),hash=digest,summary=next(line for line in text.splitlines() if line.startswith('SEEDS '))))
result=dict(result='PASS',levels=levels,scope='Shared retained class scalar after archive closure and existing exact/undersized budget checks; compare with independently audited installed declarations. No recovery playback scheduling claim.')
(root/'artifacts/unholster-retained.json').write_text(json.dumps(result,indent=2));print(result)
