"""Check campaign scene link telemetry against independent authored inventories."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('report',type=Path);args=p.parse_args()
root=Path(__file__).resolve().parents[1];report=json.loads(args.report.read_text())
assert report['result']=='PASS'
def records(name):
 rows=json.loads((root/'artifacts'/name).read_text())['results']
 row=next((r for r in rows if r['file'].lower()==report['level'].lower() and r['archive']==report['archive']),None)
 return row['records'] if row else []
events=records('events.json');triggers=records('triggers.json')
groups=[g for g in records('moving-groups.json') if g['keys']]
uids=[r['uid'] for r in events+triggers]+[g['keys'][0]['uid'] for g in groups]
keys=[(k['uid'],len(events)+len(triggers)+i) for i,g in enumerate(groups) for k in g['keys']]
assert report['campaign_groups'][:2]==[len(groups),len(keys)]
def handle(i):return ((i+1)<<16)|i
def check(rows,label):
 result=[0,0,0,2166136261];key_hits=0
 for r in rows:
  for uid in r['links']:
   index=next((i for i,value in enumerate(uids) if uid!=0xffffffff and uid==value),None)
   if index is not None:value,kind=handle(index),1
   else:
    found=next(((i,owner) for i,(value,owner) in enumerate(keys) if value==uid),None)
    if found is None:value,kind,index=uid,0,0xffffffff
    else:index,owner=found;value,kind=handle(owner),2;key_hits+=1
   result[0]+=1;result[1 if kind else 2]+=1
   for word in (uid,value,kind,index):result[3]=((result[3]^word)*16777619)&0xffffffff
 assert report[label]==result,(label,report[label],result)
 return dict(counters=result,key_owner_links=key_hits)
result=dict(result='PASS',level=report['level'],controllers=len(groups),keys=len(keys),
 trigger_links=check(triggers,'campaign_links'),event_links=check(events,'campaign_event_links'),
 scope='Authored inventories independently predict scene registration order and ordered link hashes; native replay also compares PC telemetry. Activation and whole-world original handle order excluded.')
output=args.report.parent/'scene-group-links-verification.json';output.write_text(json.dumps(result,indent=2)+'\n');print(result)
