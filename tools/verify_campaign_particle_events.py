"""Replay installed Particle_State records through shared runtime activation and scheduling."""
import json,subprocess,math
from pathlib import Path
root=Path(__file__).resolve().parents[1]
events=json.loads((root/'artifacts/events.json').read_text())['results']
emitters=json.loads((root/'artifacts/level-emitters.json').read_text())['results']
results=[]
for level in events:
 records=[r for r in level['records'] if r['type_index']==39]
 if not records:continue
 owned=next((e['records'] for e in emitters if e['file']==level['file']),[])
 lines=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--campaign-particle-events',str(root/'Installed_Game'/level['archive']),level['file'],str(root/'Installed_Game'),'1048576'],text=True).splitlines()
 assert lines[0]==f'EVENTS {len(records)} {len(owned)}',lines
 at=1
 for event in records:
  assert lines[at]=="TRIGGER 1 64 2 150 123",lines[at];at+=1
  delay=math.floor(event['delay']*1000+.5);deadline=100+delay if event['delay']>0 else -1;fired=deadline if deadline>=0 else 100
  assert lines[at]==f"EVENT {event['uid']} {deadline} {fired}",(level['file'],lines[at],deadline);at+=1
  changed=[]
  for emitter in owned:
   label,uid,enabled,stamp,prior=lines[at].split();at+=1
   assert label=='EMITTER' and int(uid)==emitter['uid']
   expected=emitter['uid'] in event['links']
   assert int(enabled)==int(expected) and int(stamp)==(fired if expected else int(prior)),(level['file'],event['uid'],uid,stamp)
   if expected:changed.append(int(uid))
  missing=[uid for uid in event['links'] if not any(e['uid']==uid for e in owned)]
  results.append(dict(level=level['file'],event=event['uid'],delay=event['delay'],fire_ms=fired,enabled_uids=changed,missing_uids=missing))
 assert at==len(lines)
report=dict(result='PASS',levels=len({r['level'] for r in results}),events=len(results),enabled_targets=sum(len(r['enabled_uids']) for r in results),scope='Installed event and emitter loaders plus shared Particle_State runtime activation/tick integration. Controlled precondition disables all emitters; test root fires one actual authored event at 100 ms, checks delay-minus-one and expiry, verifies targets and preserved unrelated deadlines. Does not verify campaign trigger eligibility, original full loader/link conversion, rendering or native XEMU.',results=results)
(root/'artifacts/campaign-particle-events-verification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
