"""Exercise transferred corpse playback/evaluation on authored PC assets."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];exe=root/'build/pc/Release/rf_npc_residency_tests.exe'
rows=[]
for level in ['L1S1.rfl','L1S2.rfl','L1S3.rfl']:
 result=subprocess.run([str(exe),'--corpse-authored',*[str(root/'Installed_Game'/name) for name in ['levels1.vpp','tables.vpp','meshes.vpp','motions.vpp']],level],cwd=root,capture_output=True,text=True,check=True)
 print(level,result.stdout.strip(),flush=True);rows.append(dict(level=level,output=result.stdout))
report=dict(result='PASS',pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),levels=rows,scope='PC owned corpse constructor/update/destructor with retained class metadata and startup class spheres. Authored model/clip transfer, names/body/registry/list ownership,120 frames per selected skeleton, authored eye/spine file-tag lookup and world placement from the transferred pose, transition and item-pose callback, temperature decay and complete resource retirement. Dynamic actor fields/material coefficients are fixture inputs; collision/source effects observed only; runtime virtual tags and surface query/color ownership remain open. No item creation, original-frame comparison, live death dispatch, rendered visual or XEMU claim.')
(root/'artifacts/corpse-authored.json').write_text(json.dumps(report,indent=2)+'\n')
