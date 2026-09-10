"""Validate the explicitly partial startup dispatcher against authored graphs."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];probe=root/'build/pc/Release/rf_event_probe.exe'
assert subprocess.check_output([str(probe),'--startup-recursion'],text=True).strip()=='PASS 5 startup recursion fixtures'
assert subprocess.check_output([str(probe),'--event-ticks'],text=True).strip()=='PASS 4 delayed event fixtures'
assert subprocess.check_output([str(probe),'--particle-events'],text=True).strip()=='PASS 4 scheduled Particle_State modes and immediate startup activation'
assert subprocess.check_output([str(probe),'--runtime-trigger-fire'],text=True).strip()=='PASS runtime trigger dispatch, self-disable, gravity and limit mark'
events=json.loads((root/'artifacts/events.json').read_text())['results'];triggers=json.loads((root/'artifacts/triggers.json').read_text())['results'];results=[]
for level in triggers:
 ev=next(e for e in events if e['file']==level['file'] and e['archive']==level['archive'])['records'];records=ev+level['records'];wanted=[0]*9;gravity=9.8
 flags=[bool(t['tail_flag']) for t in level['records']]
 def dispatch(uid,depth=0,on=True):
  global gravity
  index=next((i for i,o in enumerate(records) if o['uid']==uid and uid!=0xffffffff),None)
  if index is None:wanted[4]+=1;return
  if index>=len(ev):flags[index-len(ev)]=not on;return
  assert depth<64, (level['file'],'immediate recursion limit')
  e=ev[index];wanted[1]+=1
  if e['delay']>0:wanted[8]+=1;return
  if e['type_index']==3:
   for target in e['links']:dispatch(target,depth+1,not on)
   return
  if e['type_index']==44:
   if on:gravity=e['values'][0];wanted[2]+=1
  elif e['type_index']==48:pass
  else:wanted[3]+=1
  if e['type_index'] not in (2,3,32,36,66,69,89):
   for target in e['links']:dispatch(target,depth+1,on)
 for i,r in enumerate(level['records']):
  if r['flags'][3]!=1 or flags[i]:continue
  if r['script']:wanted[6]+=1;continue
  wanted[0]+=1
  for uid in r['links']:dispatch(uid)
 expected=wanted+list(struct.unpack('<4I',struct.pack('<4f',gravity,0,-gravity,0)))
 output=subprocess.check_output([str(probe),'--startup-events',str(root/'Installed_Game'/level['archive']),level['file']],text=True)
 got=list(map(int,next(l for l in output.splitlines() if l.startswith('STARTUP')).split()[1:]));assert got==expected,(level['file'],got,expected)
 results.append(dict(level=level['file'],counters=wanted,gravity=gravity))
assert [r['gravity'] for r in results if r['counters'][2]]==[4.0,3.0,4.0,9.800000190734863]
report=dict(result='PASS',levels=len(results),gravity_events=sum(r['counters'][2] for r in results),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),scope='Partial runtime dispatch integration against raw graph expectations. Known Set_Gravity applies; unsupported actions/targets, unresolved links, script gates and delayed events remain explicitly counted. Full original campaign execution not claimed.',results=results)
(root/'artifacts/runtime-startup-verification.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
