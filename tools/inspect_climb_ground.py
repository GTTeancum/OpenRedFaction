"""Measure authored static floor surfaces around the first L1S2 movement region."""
import json,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];combined='--movers' in sys.argv
region=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--level-regions',str(root/'Installed_Game/levels1.vpp'),'L1S2.rfl'])
v=struct.unpack('<I15f',region[:64]);center=v[1:4]
queries=[(center[0]+dx,y,center[2]+dz) for y in (20,0,-2,-10) for dz in (-4,-2,-1,0,1,2,4) for dx in (-4,-2,-1,0,1,2,4)]
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--scene-rays' if combined else '--world-rays',str(root/'Installed_Game/levels1.vpp'),'L1S2.rfl'],input=b''.join(struct.pack('<6f',*q,0,-20,0) for q in queries))
assert len(raw)==len(queries)*48
rows=[]
for i,q in enumerate(queries):
 status,matched,fraction,x,y,z,nx,ny,nz,face,room,hits=struct.unpack_from('<iI7f3I',raw,i*48)
 assert status==0
 rows.append(dict(start=q,matched=bool(matched),point=[x,y,z],normal=[nx,ny,nz],face=face,room=room,mover=hits if combined and matched and hits!=0xffffffff else None))
report=dict(region=dict(kind=v[0],center=center,matrix=v[4:13],size=v[13:16]),rays=rows,scope='Initial static/mover rays' if combined else 'Static-only rays; neither proves character clearance or traversal')
out=root/'artifacts/climb-ground';out.mkdir(exist_ok=True);(out/('movers.json' if combined else 'report.json')).write_text(json.dumps(report,indent=2))
from collections import Counter
print(json.dumps(dict(rays=len(rows),surfaces=[dict(face=k[0],height=k[1],hits=n) for k,n in Counter((r['face'],round(r['point'][1],3)) for r in rows if r['matched']).items()]),indent=2))
