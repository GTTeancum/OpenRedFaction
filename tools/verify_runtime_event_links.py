"""Check owned event targets against authored UIDs in the current scene registry."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];probe=root/'build/pc/Release/rf_event_probe.exe'
events=json.loads((root/'artifacts/events.json').read_text())['results']
triggers=json.loads((root/'artifacts/triggers.json').read_text())['results']
total=resolved=0;results=[]
for level in events:
 trigger=next(t for t in triggers if t['file']==level['file'] and t['archive']==level['archive'])
 records=level['records']+trigger['records'];wanted=[];found=0
 for event in level['records']:
  for uid in event['links']:
   index=next((i for i,o in enumerate(records) if o['uid']==uid and uid!=0xffffffff),None)
   wanted.append([event['uid'],uid,uid,0,0xffffffff] if index is None else [event['uid'],uid,((index+1)<<16)|index,1,index])
   found+=index is not None
 output=subprocess.check_output([str(probe),'--event-links',str(root/'Installed_Game'/level['archive']),level['file']],text=True)
 assert [list(map(int,line.split())) for line in output.splitlines()]==wanted,level['file']
 total+=len(wanted);resolved+=found
 results.append(dict(level=level['file'],links=len(wanted),resolved=found))
report=dict(result='PASS',levels=len(events),links=total,resolved=resolved,unresolved=total-resolved,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),scope='Owned event links against authored inventories and event-then-trigger fixture registration. Missing families retain unresolved UIDs. No original whole-world handle ordering, dispatch or propagation equivalence.',results=results)
(root/'artifacts/runtime-event-links-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='results'})
