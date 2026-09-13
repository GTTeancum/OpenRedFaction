"""Profile authored SP spawn angular branches using shared constructor/dispatch."""
import collections,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game';exe=root/'build/pc/Release'
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
descriptors=[struct.unpack('<8I',subprocess.check_output([str(exe/'rf_movement_probe.exe'),'--descriptor',str(game/'tables.vpp'),str(i)])) for i in range(16)]
names=['ordinary','external','physics_49e180','mode15','player_look','skip'];levels=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 text=subprocess.check_output([str(exe/'rf_entity_assets_probe.exe'),'--seeds',str(game/'levels1.vpp'),str(game/'tables.vpp'),level],text=True)
 rows=[line.split('\t') for line in text.splitlines() if line.startswith('SEED_PHYSICS\t')];assert rows
 flag_input=b''.join(w(*[int(x) for x in row[3:7]],0) for row in rows)
 flags=struct.unpack('<'+'I'*len(rows),subprocess.check_output([str(exe/'rf_entity_probe.exe'),'--physics-flags'],input=flag_input))
 requests=[]
 for row,flag in zip(rows,flags):
  requested=int(row[7]);slot=requested if requested<16 and descriptors[requested][0]&255 else 0
  control=int(bool(int(row[4])&0x4000 and int(row[3])&1))
  requests.append(w(flag,descriptors[slot][1],control,0))
 routes=struct.unpack('<'+'I'*len(rows),subprocess.check_output([str(exe/'rf_movement_probe.exe'),'--angular-route'],input=b''.join(requests)))
 counts=collections.Counter(names[r] for r in routes);classes={}
 for row,flag,route in zip(rows,flags,routes):
  key=row[2];value=classes.setdefault(key,dict(count=0,routes={},body_flags=[]));value['count']+=1
  label=names[route];value['routes'][label]=value['routes'].get(label,0)+1
  if flag not in value['body_flags']:value['body_flags'].append(flag)
 levels.append(dict(level=level,spawn_records=len(rows),routes=dict(counts),classes=classes))
report=dict(result='PASS',levels=levels,scope='Authored spawn classification, SP nonprimary identity, constructor flags and enabled descriptor fallback. Includes all entity records, not only registered/renderable NPCs. Does not measure runtime branch changes or execute special angular routines.')
(root/'artifacts/authored-angular-routes.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
