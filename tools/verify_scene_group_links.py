"""Check campaign scene link telemetry against independent authored inventories."""
import argparse,json,struct,subprocess
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
movers=records('movers.json') if 'campaign_movers' in report else []
if 'campaign_movers' in report:
 assert report['campaign_movers'][0]==len(movers) and report['campaign_movers'][2]==(44 if 'campaign_memberships' in report else 24)*len(movers)
 uids.extend(r['uid'] for r in movers)
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
if 'campaign_memberships' in report:
 raw_groups=records('moving-groups.json')
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--registered-member-groups',str(root/'Installed_Game'/report['archive']),report['level']])
 group_count,mover_count,owned,peak=struct.unpack_from('<4I',raw);assert mover_count==len(movers) and group_count==len(raw_groups)
 controller_slots={i:len(events)+len(triggers)+sum(bool(g['keys']) for g in raw_groups[:i]) for i,g in enumerate(raw_groups) if g['keys']}
 def remap(value):
  if value==0xffffffff:return value
  slot=value&0xffff
  return handle(len(events)+len(triggers)+len(groups)+slot if slot<mover_count else controller_slots[slot-mover_count])
 words=[]
 for i in range(mover_count):
  uid,kind,mover,parent,flags=struct.unpack_from('<5I',raw,16+i*20);words.extend([uid,kind,remap(mover),remap(parent),flags])
 offset=16+mover_count*20;link_count=0
 for i in range(group_count):
  count,sign=struct.unpack_from('<2I',raw,offset);offset+=8;words.extend([count,sign]);link_count+=count
  for j in range(count):words.append(remap(struct.unpack_from('<I',raw,offset)[0]));offset+=4
 assert offset==len(raw)
 binding_hash=2166136261
 for word in words:binding_hash=((binding_hash^word)*16777619)&0xffffffff
 assert report['campaign_memberships']==[group_count,link_count,owned,peak,binding_hash],report['campaign_memberships']
result=dict(result='PASS',level=report['level'],controllers=len(groups),keys=len(keys),movers=len(movers),memberships=report.get('campaign_memberships'),
 trigger_links=check(triggers,'campaign_links'),event_links=check(events,'campaign_event_links'),
 scope='Authored inventories predict scene registration/link hashes; membership probe is remapped to scene handles and checks ordered parents/flags/members. Native replay compares PC telemetry. Activation and whole-world original handle order excluded.')
output=args.report.parent/'scene-group-links-verification.json';output.write_text(json.dumps(result,indent=2)+'\n');print(result)
