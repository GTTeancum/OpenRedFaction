"""Compare shared-model base ownership with the verified archive-backed path."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
resources=json.loads((root/'artifacts/static-render-resource.json').read_text())
assert resources['result']=='PASS'
for record in resources['records']:
 p=subprocess.run([str(root/'build/pc/Release/rf_clutter_shared_tests.exe'),str(root/'Installed_Game/meshes.vpp'),record['model']],capture_output=True)
 assert p.returncode==0,(record['model'],p.stdout,p.stderr)
report=dict(result='PASS',models=len(resources['records']),body_comparisons=len(resources['records'])*2,
 scope='PC actual archive resources, shared versus archive-backed base owner bytes (identity pointers/accounting normalized) and every retained physics sphere; physics0/20, two concurrent borrowers, distinct sphere allocations, exact/short budgets, missing model rollback, reference overflow, ordered destruction and repeated close. No live scene or native XEMU claim.')
(root/'artifacts/clutter-shared-bases.json').write_text(json.dumps(report,indent=2));print(report)
