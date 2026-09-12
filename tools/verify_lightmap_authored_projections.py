"""Check saved projections and measure agreement with authored corner UVs."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];exe=root/'build/pc/Release/rf_collision_probe.exe'
levels=json.loads((root/'artifacts/levels.json').read_text());rows=[]
for level in levels:
 result=subprocess.run([str(exe),'--level-lightmap-projections',str(root/'Installed_Game'/level['archive']),level['file']],capture_output=True,text=True,check=True)
 values=result.stdout.split();assert len(values)==4,result.stdout
 rows.append(dict(level=level['file'],mappings=int(values[0]),corners=int(values[1]),components_over_1e4=int(values[2]),max_difference=float(values[3])))
report=dict(result='PASS',levels=len(rows),mappings=sum(r['mappings'] for r in rows),corners=sum(r['corners'] for r in rows),components_over_1e4=sum(r['components_over_1e4'] for r in rows),max_difference=max(r['max_difference'] for r in rows),pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='All inventoried installed static geometry mappings parse through shared accessor/projector. Corner UV difference is measured evidence, not asserted equivalence; original loader/texel ownership, movers, live effects and rendering are not executed.',rows=rows)
(root/'artifacts/lightmap-authored-projections.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='rows'},indent=2))
