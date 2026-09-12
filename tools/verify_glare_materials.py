"""Archive-backed glare material composition versus independently loaded textures."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];audit=json.loads((root/'artifacts/glare-bitmaps/report.json').read_text())
archives=sorted({m['archive'] for t in audit['textures'] for m in t['matches']})
probe=root/'build/pc/Release/rf_material_probe.exe'
def run(budget,paths=archives):
 return subprocess.check_output([str(probe),'--glare-materials',str(root/'Installed_Game/tables.vpp'),str(budget),*[str(root/'Installed_Game'/a) for a in paths]]).decode().splitlines()
rows=run(4000000);header=list(map(int,rows[0].split()));assert header[:3]==[0,56,38]
resident=24+56*(12+3*84)+audit['unambiguous_animation_resident_bytes']-38*20
assert header[3]==resident
assert run(resident)==rows and run(resident-1)==['-4 0 0 0']
assert run(4000000,['tables.vpp'])==['-3 0 0 0']
names=[r['name'] for r in audit['textures']];lookup={name.lower():i for i,name in enumerate(names)}
for row,cls in zip(rows[1:57],audit['class_bindings']):
 expected=[lookup[cls['bitmaps'][key].lower()] if key in cls['bitmaps'] else 0xffffffff for key in ('corona','volumetric','reflection')]
 assert row=='B '+' '.join(map(str,expected))
at=57;frames=0
for texture in audit['textures']:
 name=texture['name'];archive=texture['matches'][0]['archive']
 single=list(map(int,subprocess.check_output([str(probe),'--particle-animation',name,'4000000',str(root/'Installed_Game'/archive)]).split()))
 assert not single[0]
 _,owner,image_owner,n,rate,bytes_used,index=single[:7]
 assert rows[at]==f'T {name} {n} {rate} {archives.index(archive)} {bytes_used}';at+=1
 for f in range(n):
  assert rows[at]=='F '+' '.join(map(str,single[7+3*f:10+3*f]));at+=1;frames+=1
assert at==len(rows)
report=dict(result='PASS',classes=56,textures=38,frames=frames,resident_bytes=resident,scope='PC owned bundle after classes/archive retirement, all class slot mappings and every frame hash versus independent texture loads, exact/short budgets and missing archive failure. No original bitmap handle IDs, frame clock or native XEMU claim.')
(root/'artifacts/glare-materials.json').write_text(json.dumps(report,indent=2));print(report)
