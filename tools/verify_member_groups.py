"""Owned mover membership composition vs independent authored UID inventory."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];groups=json.loads((root/'artifacts/moving-groups.json').read_text())['results'];movers={(l['archive'],l['file']):l for l in json.loads((root/'artifacts/movers.json').read_text())['results']};results=[]
for level in groups:
 records=movers[(level['archive'],level['file'])]['records'];n=len(records);ng=len(level['records']);objects=[[r['uid'],9,0x12340000+i,0xffffffff,0x6000000] for i,r in enumerate(records)];members=[];total=0;maxrefs=0
 for gi,g in enumerate(level['records']):
  flags=0x80000100 if g['flags'][2] else 0x80002000
  if g['flags'][1]:flags|=4
  if g['flags'][5]:flags|=0x1000
  ids=g['ids2'] if g['keys'] else [];total+=len(ids);maxrefs=max(maxrefs,len(ids));handles=[];sign=1.
  for uid in ids:
   match=next((o for o in objects if o[0]&0xffffffff==uid and uid!=0xffffffff and (uid!=0xfffffc19 or not o[4]&2)),None)
   if match is None or match[1]!=9:continue
   handles.append(match[2]);match[3]=0x23450000+n+gi
   if flags&0x1000:match[4]|=0x40000
   if flags&4 and not flags&0x2100:sign=-sign
  members.append((handles,sign))
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--member-groups',str(root/'Installed_Game'/level['archive']),level['file']]);count,object_count,retained,peak=struct.unpack_from('<4I',raw)
 assert (count,object_count,retained,peak)==(ng,n,20+12*ng+4*total,20+12*ng+4*total+20*n+4*maxrefs)
 at=16
 for o in objects:assert raw[at:at+20]==struct.pack('<i4I',*o),(level['file'],'parent/flags',o);at+=20
 for handles,sign in members:
  size=8+len(handles)*4;want=struct.pack('<If'+'I'*len(handles),len(handles),sign,*handles)
  assert raw[at:at+size]==want,(level['file'],'membership');at+=size
 assert at==len(raw)
 results.append(dict(file=level['file'],groups=count,movers=n,accepted=sum(len(h) for h,s in members),retained=retained,peak=peak))
report=dict(result='PASS',levels=len(results),groups=sum(r['groups'] for r in results),movers=sum(r['movers'] for r in results),accepted=sum(r['accepted'] for r in results),max_retained=max(r['retained'] for r in results),max_peak=max(r['peak'] for r in results),scope='PC composition over authored mover UID lists using diagnostic handles: ordered membership, duplicate references, final parent/flag writes and rotation flip parity. Exact/short budgets preserve objects/output; source archive closed. General objects, real handle allocation and scene activation remain open.',results=results)
(root/'artifacts/member-groups-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
