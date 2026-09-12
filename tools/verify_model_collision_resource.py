"""Complete owned static collision resources against installed metadata."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];models=owned=parts=lods=peak=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if not entry['name'].lower().endswith(('.v3c','.v3m')):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:stream.seek(entry['offset']);raw=stream.read(entry['size'])
  sections=[s for s in inspect(raw)['sections'] if 'lods' in s];all_lods=[l for s in sections for l in s['lods']]
  expected=[]
  if any(not l['flags']&32 for l in all_lods):expected=['N']
  else:
   budget=20+len(sections)*44+sum(20+l['data_bytes']+l['batches']*20 for l in all_lods)
   expected=[f'R {len(sections)} {len(all_lods)} {budget}'];first=0
   for i,section in enumerate(sections):
    count=len(section['lods']);offset=section['offset']+8+56+count*4;metadata=raw[offset:offset+12]+raw[offset+16:offset+40]
    expected.append(f'B {i} {first+count-1} {first} '+metadata.hex());first+=count
   owned+=1;peak=max(peak,budget);parts+=len(sections);lods+=len(all_lods)
  output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--collision-resource'],text=True)
  assert [line for line in output.splitlines() if line=='N' or line.startswith(('R ','B '))]==expected,entry['name']
  models+=1
report=dict(result='PASS',models=models,owned=owned,parts=parts,lods=lods,peak_resource_bytes=peak,scope='All installed complete static resources: shared bounds/offsets and selected/fallback LOD identity, exact total budget and one-byte-under rejection, repeated close, late malformed-LOD unwind; animated rejection. PC archive test, native build only; no live scene/XEMU.')
(root/'artifacts/model-collision-resource.json').write_text(json.dumps(report,indent=2));print(report)
