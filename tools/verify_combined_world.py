"""Combined loaded world/mover query ownership and visibility replay."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];levels=json.loads((root/'artifacts/geometry.json').read_text());results=[]
for level in levels:
 run=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--combined-world',str(root/'Installed_Game'/level['archive']),level['file']],capture_output=True)
 assert run.returncode==0,(level['file'],run.returncode,run.stderr)
 rooms,movers,queries,hits,moving_hits,static_hits,checksum,retained=map(int,run.stdout.split());assert rooms==level['rooms'] and hits==moving_hits+static_hits
 results.append(dict(file=level['file'],rooms=rooms,movers=movers,queries=queries,hits=hits,moving_hits=moving_hits,static_hits=static_hits,checksum=checksum,retained=retained))
report=dict(result='PASS',levels=len(results),queries=sum(r['queries'] for r in results),hits=sum(r['hits'] for r in results),moving_hits=sum(r['moving_hits'] for r in results),static_hits=sum(r['static_hits'] for r in results),max_retained=max(r['retained'] for r in results),scope='PC combined query at first face of each nonempty static room and every mover face; exact static-only agreement with existing world ray, all hit identities bound to owners, nullable visibility agreement, byte-identical replay after closing sources/archive and poisoning storage. Initial poses and diagnostic runtime handles; not a complete original loaded-world comparison or XEMU.',results=results)
(root/'artifacts/combined-world-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
