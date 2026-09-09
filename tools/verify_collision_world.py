"""Loaded collision world budgets, ownership, source binding and ray replay."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
levels=json.loads((root/'artifacts/geometry.json').read_text());results=[]
for level in levels:
 run=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world',str(root/'Installed_Game'/level['archive']),level['file']],capture_output=True)
 assert run.returncode==0,(level['file'],run.returncode,run.stderr)
 rooms,faces,primary,children,retained,peak,queries,hits,errors=map(int,run.stdout.split())
 assert rooms==level['rooms'] and faces==level['faces'] and errors==0,(level['file'],errors)
 results.append(dict(file=level['file'],rooms=rooms,faces=faces,primary=primary,children=children,retained=retained,peak=peak,queries=queries,hits=hits))
report=dict(result='PASS',levels=len(results),max_retained=max(r['retained'] for r in results),max_peak=max(r['peak'] for r in results),queries=sum(r['queries'] for r in results),hits=sum(r['hits'] for r in results),scope='PC loaded-world ownership and exact/insufficient peak budgets; all face IDs bound to owning rooms. One ray per nonempty room, byte-identical replay after closing geometry and allocating poison storage. Not a complete original world-loader comparison or XEMU test.',results=results)
(root/'artifacts/collision-world-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
