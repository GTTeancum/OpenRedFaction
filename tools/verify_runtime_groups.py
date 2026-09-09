"""Persistent controller runtime composition over all authored groups."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];results=[]
for level in json.loads((root/'artifacts/moving-groups.json').read_text())['results']:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--runtime-groups',str(root/'Installed_Game'/level['archive']),level['file']]);count,budget,translations,rotations=struct.unpack_from('<4I',raw)
 assert count==len(level['records']) and budget==12+324*count and len(raw)==16+320*count
 nt=nr=0
 for i,g in enumerate(level['records']):
  if not g['keys']:want=bytes(320)
  else:
   first=g['keys'][0];flags=0x80000100 if g['flags'][2] else 0x80002000
   if g['flags'][0]:flags|=2
   if g['flags'][1]:
    flags|=4
    if any(first['timing'][3:5]):flags|=0x40
   for bit in range(3,6):
    if g['flags'][bit]:flags|=1<<(bit+7)
   disk=first['orientation_disk'];matrix=disk[3:]+disk[:3];base=first['position']
   if flags&4:kind=2;nr+=1;runtime=bytes(76);position=base
   else:
    kind=1;nt+=1;index=g['unknown'];position=g['keys'][index]['position'];mode=g['mode'] if g['mode']<=5 else 1
    runtime=struct.pack('<IIiifi2fiI9f',flags,mode,index,-1,0.,-1,0.,0.,0,0x6000001,*position,*position,0.,0.,0.)
   pose=struct.pack('<I58f',0x6000001,0.,*base,*matrix,*position,*position,*position,0.,0.,0.,*matrix,*matrix,*matrix,*position,*position)
   want=struct.pack('<2I',kind,flags)+runtime+pose
  assert raw[16+320*i:336+320*i]==want,(level['file'],i,g['name'])
 assert (translations,rotations)==(nt,nr)
 results.append(dict(file=level['file'],groups=count,translations=nt,rotation_pending=nr,allocated_bytes=budget))
report=dict(result='PASS',levels=len(results),groups=sum(r['groups'] for r in results),translations=sum(r['translations'] for r in results),rotation_pending=sum(r['rotation_pending'] for r in results),max_allocated=max(r['allocated_bytes'] for r in results),scope='PC persistent composition of original-verified flags/base pose/translation initialization after archive closure. Exact/short budgets, late invalid start-key output preservation and repeated close. Rotation retains source/base/flags with explicit unavailable translation state. No handles, attachments, playback or XEMU.',results=results)
(root/'artifacts/runtime-groups-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
