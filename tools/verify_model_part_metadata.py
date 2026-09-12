"""Shared submesh metadata versus direct installed archive bytes."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];models=parts=lods=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if not entry['name'].lower().endswith(('.v3c','.v3m')):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:stream.seek(entry['offset']);raw=stream.read(entry['size'])
  expected=[];first=0
  for section in inspect(raw)['sections']:
   if 'lods' not in section:continue
   count=len(section['lods']);offset=section['offset']+8+56+count*4
   wire=raw[offset:offset+40]+struct.pack('<II',first,count)
   expected.append('H '+str(len(expected))+' '+wire.hex());first+=count
  output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--part-metadata'],text=True)
  assert [line for line in output.splitlines() if line.startswith('H ')]==expected,entry['name']
  models+=1;parts+=len(expected);lods+=first
report=dict(result='PASS',models=models,parts=parts,lods=lods,scope='All installed V3C/V3M offsets, radius, min/max and flattened LOD range equal archive bytes; invalid ordinal/truncated metadata preserve output. PC archive execution; native build only, no live binding.')
(root/'artifacts/model-part-metadata.json').write_text(json.dumps(report,indent=2));print(report)
