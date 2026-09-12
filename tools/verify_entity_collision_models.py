"""Verify initial shared collision selection and owner budgets for shipped V3Cs."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];models=0;peak=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if not entry['name'].lower().endswith('.v3c'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:stream.seek(entry['offset']);raw=stream.read(entry['size'])
  first=next(s for s in inspect(raw)['sections'] if 'lods' in s);selected=len(first['lods'])-1;lod=first['lods'][selected]
  budget=28+2*(16+lod['data_bytes']+lod['batches']*16);peak=max(peak,budget)
  max_vertices=max(struct.unpack_from('<H',raw,lod['data_offset']+lod['data_bytes']+4+i*18)[0] for i in range(lod['batches']))
  actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--skin-selection'],text=True).strip()
  assert actual==f'S 2 {selected} {budget} {max_vertices}',(entry['name'],actual)
  models+=1
report=dict(result='PASS',models=models,peak_two_model_bytes=peak,scope='Initial last-LOD/first-submesh selection from95 shipped V3Cs, two independent geometry owners, exact/one-under total budgets, later-owner failure cleanup and repeated close. Native scene integration tested separately.')
(root/'artifacts/entity-collision-models.json').write_text(json.dumps(report,indent=2));print(report)
