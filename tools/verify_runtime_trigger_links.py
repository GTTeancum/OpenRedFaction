"""Check registered event/trigger link resolution against authored inventories."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];probe=root/'build/pc/Release/rf_event_probe.exe'
events=json.loads((root/'artifacts/events.json').read_text())['results'];triggers=json.loads((root/'artifacts/triggers.json').read_text())['results'];total=resolved=0;gravity=[]
for level in triggers:
 ev=next(e for e in events if e['file']==level['file'] and e['archive']==level['archive'])['records'];records=ev+level['records'];wanted=[]
 for r in level['records']:
  for uid in r['links']:
   index=next((i for i,o in enumerate(records) if o['uid']==uid and uid!=0xffffffff),None)
   row=[r['uid'],uid,uid,0,0xffffffff] if index is None else [r['uid'],uid,((index+1)<<16)|index,1,index]
   wanted.append(row);total+=1;resolved+=index is not None
   if index is not None and index<len(ev) and ev[index]['type_index']==44:gravity.append(dict(level=level['file'],trigger=r['uid'],event=uid,handle=row[2]))
 output=subprocess.check_output([str(probe),'--trigger-links',str(root/'Installed_Game'/level['archive']),level['file']],text=True)
 assert [list(map(int,l.split())) for l in output.splitlines()]==wanted,level['file']
assert len(gravity)==4
report=dict(result='PASS',levels=len(triggers),links=total,resolved=resolved,unresolved=total-resolved,gravity_links=gravity,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),scope='Shared owner integration against raw authored inventories, event-then-trigger fixture registration order. Other object families/mover keys unregistered; no dispatch or whole-world handle parity.')
(root/'artifacts/runtime-trigger-links-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
