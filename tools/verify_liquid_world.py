"""Authored water surface queries and shared projectile continuation; no UI input."""
import json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=[]
for row in json.loads((ROOT/'artifacts/liquid-geometry-inventory.json').read_text())['rows']:
 if not row['liquid_rooms']:continue
 out=subprocess.check_output([str(ROOT/'build/pc/Release/rf_collision_probe.exe'),'--world-liquid',str(ROOT/'Installed_Game'/row['archive']),row['file']],text=True)
 q,w,s,e,t=map(int,out.split());rows.append(dict(level=row['file'],queries=q,water_hits=w,solid_hits=s,entries=e,water_then_solid=t))
report={key:sum(r[key] for r in rows) for key in ['queries','water_hits','solid_hits','entries','water_then_solid']}
assert report['queries']==1300 and report['entries']>0 and report['water_then_solid']>0
report['rows']=rows
(ROOT/'artifacts/liquid-world-sweep.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})
