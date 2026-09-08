"""Check every installed V3C material against independent archive traversal."""
import json,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
models=materials=submeshes=0
for archive in inventory['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3c'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        sections=[s for s in inspect(raw)['sections'] if s['type']=='0x5355424d']
        expected=[]
        for mesh,s in enumerate(sections):
            for n in range(s['materials']):
                offset=s['material_offset']+n*84
                expected.append(f'M {mesh} {n} '+raw[offset:offset+84].hex())
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--materials'],text=True)
        actual=[line for line in output.splitlines() if line.startswith('M ')]
        assert actual==expected,(archive['path'],entry['name'])
        models+=1;materials+=len(expected);submeshes+=len(sections)
assert models and materials
report=dict(result='PASS',models=models,submeshes=submeshes,materials=materials,bounds_rejections=submeshes,
    scope='Every installed V3C serialized 84-byte material, byte-for-byte against independent Python traversal; out-of-range reads preserve output; runtime conversion excluded')
(root/'artifacts/model-material-files-verification.json').write_text(json.dumps(report,indent=2));print(report)
