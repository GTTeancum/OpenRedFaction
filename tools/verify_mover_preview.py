"""Transformed mover projection vs the same meshes baked into world space."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];results=[]
for level in json.loads((root/'artifacts/movers.json').read_text())['results']:
 values=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-preview',str(root/'Installed_Game'/level['archive']),level['file']]).split()))
 assert len(values)==4 and values[0]==level['count'] and values[1]==3*level['count']
 results.append(dict(file=level['file'],movers=values[0],pose_cases=values[1],vertices=values[2],changed_shifted_meshes=values[3]))
report=dict(result='PASS',levels=len(results),movers=sum(r['movers'] for r in results),pose_cases=sum(r['pose_cases'] for r in results),vertices=sum(r['vertices'] for r in results),changed_shifted_meshes=sum(r['changed_shifted_meshes'] for r in results),scope='PC transformed preview vs existing projection of world-baked geometry: full vertex bytes, face normal shading, UV/lightmap preservation, material base remapping, identity/authored/shifted poses. Exact/short mesh budgets. Synthetic inspection camera; not original renderer fidelity or scene material integration.',results=results)
(root/'artifacts/mover-preview-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
