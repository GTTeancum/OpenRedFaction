"""Exercise campaign event ownership, exact budgets and handle cleanup on PC."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];probe=root/'build/pc/Release/rf_event_probe.exe';results=[]
for level in json.loads((root/'artifacts/events.json').read_text())['results']:
 output=subprocess.check_output([str(probe),'--owned',str(root/'Installed_Game'/level['archive']),level['file']],text=True)
 count,allocated=map(int,output.split());assert count==len(level['records'])
 results.append(dict(level=level['file'],events=count,allocated_bytes=allocated))
report=dict(result='PASS',levels=len(results),events=sum(r['events'] for r in results),peak_bytes=max(r['allocated_bytes'] for r in results),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),scope='Shared ownership/registration with installed records; handle lookup/removal, repeated close, exact-budget success and one-byte-short rejection, record access after archive close; type32 Switch initialization and distinct mutable storage, no type-specific allocation for other types. Full original factory and event actions excluded.',results=results)
(root/'artifacts/runtime-events-verification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
