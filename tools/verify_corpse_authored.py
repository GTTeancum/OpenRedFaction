"""Exercise transferred corpse playback/evaluation on authored PC assets."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];exe=root/'build/pc/Release/rf_npc_residency_tests.exe'
rows=[]
for level in ['L1S1.rfl','L1S2.rfl','L1S3.rfl']:
 result=subprocess.run([str(exe),'--corpse-authored',*[str(root/'Installed_Game'/name) for name in ['levels1.vpp','tables.vpp','meshes.vpp','motions.vpp']],level],cwd=root,capture_output=True,text=True,check=True)
 print(level,result.stdout.strip(),flush=True);rows.append(dict(level=level,output=result.stdout))
report=dict(result='PASS',pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),levels=rows,scope='PC scene model detach, reset/start/duration,120 playback/evaluation/followpoint frames per unique skeleton with death_generic and final reference retirement. Original actor matrices poisoned after transfer. Authored clip loading under bounded residency. No original-frame comparison, full corpse constructor, live death dispatch, rendered visual or XEMU claim.')
(root/'artifacts/corpse-authored.json').write_text(json.dumps(report,indent=2)+'\n')
