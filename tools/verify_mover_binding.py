"""Initial owned mover bindings, pose/radius evidence, budgets and identity separation."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];levels=json.loads((root/'artifacts/movers.json').read_text())['results'];bounds=json.loads((root/'artifacts/mover-bounds-original.json').read_text())['results'];results=[]
for level,bound in zip(levels,bounds):
 assert level['file']==bound['file'] and level['archive']==bound['archive']
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--bound-movers',str(root/'Installed_Game'/level['archive']),level['file']]);count,retained,peak=struct.unpack_from('<3I',raw);assert count==level['count'] and len(raw)==12+368*count and peak>=retained
 for i,(record,b) in enumerate(zip(level['records'],bound['records'])):
  uid,handle,faces=struct.unpack_from('<i2I',raw,12+368*i);assert uid==record['uid']==b['uid'] and handle==0x12340000+i and faces==record['faces']
  p=record['position'];disk=record['orientation_disk'];matrix=disk[3:]+disk[:3];r=b['origin_radius']
  want=struct.pack('<30f',*[v-r for v in p],*[v+r for v in p],*p,*matrix,*p,*matrix)
  assert raw[24+368*i:144+368*i]==want,(level['file'],uid,'bounds/pose')
  pose=struct.pack('<I58f',0x6000000,r,*p,*matrix,*p,*p,*p,0.,0.,0.,*matrix,*matrix,*matrix,*[v-r for v in p],*[v+r for v in p])
  assert raw[144+368*i:380+368*i]==pose,(level['file'],uid,'owned base/runtime pose')
 results.append(dict(file=level['file'],count=count,retained=retained,peak=peak))
report=dict(result='PASS',levels=len(results),movers=sum(r['count'] for r in results),max_retained=max(r['retained'] for r in results),max_peak=max(r['peak'] for r in results),scope='PC owned collision views and 236-byte factory/physics pose snapshots after source closure: exact base/current/pending poses, matrices, initial flags, zero velocity and original-radius bounds; diagnostic runtime handles separate from file UIDs. Exact/short peak budgets and failure preservation. No runtime handle allocator, full factory, changing poses or XEMU.',results=results)
(root/'artifacts/mover-binding-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
