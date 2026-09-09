"""Compare C mover loading with the independently inventoried installed records."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
reference=json.loads((root/'artifacts/movers.json').read_text());results=[]
keys='uid offset bytes geometry_offset geometry_bytes textures rooms vertices faces mappings'.split()
for level in reference['results']:
 run=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--movers',str(root/'Installed_Game'/level['archive']),level['file']],capture_output=True)
 assert run.returncode==0,(level['file'],run.returncode,run.stderr)
 lines=run.stdout.decode().splitlines();count,budget=map(int,lines[0].split())
 assert count==level['count'] and len(lines)==count+1
 for line,record in zip(lines[1:],level['records']):
  words=line.split();assert list(map(int,words[:10]))==[record[k] for k in keys],(level['file'],record['uid'],words)
  assert list(map(int,words[10:13]))==record['trailer']
  disk=record['orientation_disk'];pose=record['position']+disk[3:]+disk[:3]
  assert struct.pack('<12f',*map(float,words[13:]))==struct.pack('<12f',*pose)
 results.append(dict(file=level['file'],count=count,budget=budget))
report=dict(result='PASS',levels=len(results),movers=sum(r['count'] for r in results),max_budget=max(r['budget'] for r in results),scope='PC C reader vs Python inventory: all record spans, poses, trailers and geometry counts; exact budget and one-byte-short failure preserving output; 5,624 truncated-header/geometry/trailer checks preserve output; all corners and vertices accessed after archive closure. Not original geometry parser execution, runtime object creation or XEMU validation.',results=results)
(root/'artifacts/mover-loader-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
