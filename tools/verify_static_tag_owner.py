"""Retained static attachments versus serialized first-submesh first-LOD data."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];records=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for e in archive.get('vpp',{}).get('entries',[]):
  if not e['name'].lower().endswith('.v3m'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  first=next(s for s in inspect(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
  start=first['attachment_offset'];count=first['props'];expected=[];names=[]
  for i in range(count):
   r=raw[start+i*100:start+(i+1)*100];names.append(r[:68].split(b'\0',1)[0])
   expected.append(r[:68]+bytes(4)+r[68:])
  output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),e['name'],'--static-tags'])
  assert struct.unpack_from('<3I',output)==(count,12+104*count,104),e['name']
  assert len(output)==12+108*count
  for i,r in enumerate(expected):
   assert output[12+i*108:116+i*108]==r,(e['name'],i,'record')
   index=next(j for j,n in enumerate(names) if n.lower().startswith(names[i].lower()))
   assert struct.unpack_from('<i',output,116+i*108)[0]==index,(e['name'],i,'lookup')
  records.append(dict(model=e['name'],attachments=count,bytes=12+104*count))
report=dict(result='PASS',models=len(records),attachments=sum(r['attachments'] for r in records),
 maximum_bytes=max(r['bytes'] for r in records),records=records,
 scope='All installed static models: serialized first-submesh first-LOD names/rotation/position/parent bytes versus retained PC owner after archive close and directory poisoning; exact/one-byte-short budgets, lookup and repeated close. No original attachment-loader, parent pose, NXDK allocation failure or native XEMU proof.')
(root/'artifacts/static-tag-owner.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
