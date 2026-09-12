"""Check post-conversion texture replacement without changing base material flags."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
probe=root/'build/pc/Release/rf_material_probe.exe'
archives=[str(game/n) for n in ('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')]
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
entries={e['name'].lower():e for a in inv if a['path']=='meshes.vpp' for e in a['vpp']['entries']}
names=[r['model'] for r in json.loads((root/'artifacts/static-render-resource-nxdk.json').read_text())['records']]
rows=[]
with (game/'meshes.vpp').open('rb') as f:
 for name in names:
  e=entries[name.lower()];f.seek(e['offset']);data=f.read(e['size'])
  for s in inspect(data)['sections']:
   if s['type']=='0x5355424d':
    rows.extend(data[s['material_offset']+i*84:s['material_offset']+(i+1)*84] for i in range(s['materials']))
def run(overrides,budget=4194304):
 payload=struct.pack('<I',len(rows))+b''.join(rows)
 if overrides is not None:payload+=b''.join((n or '').encode().ljust(64,b'\0') for n in overrides)
 return subprocess.run([str(probe),'--records' if overrides is None else '--records-overrides',str(budget),*archives],input=payload,capture_output=True)
def parse(result):
 assert result.returncode==0,result.stdout
 lines=result.stdout.splitlines();return list(map(int,lines[0].split())),[(bytes.fromhex(l.split()[1].decode()),l.split()[2]) for l in lines[1:]]
base_header,base=parse(run(None));catalog=[]
for row in rows:
 for offset in (0,48):
  name=row[offset:offset+32].split(b'\0')[0].decode()
  if name and name.lower() not in [n.lower() for n in catalog]:catalog.append(name)
assert len(catalog)==base_header[1]
cases=0
for overrides in ([None]*len(rows),[catalog[(i+1)%len(catalog)].upper() for i in range(len(rows))],
                  [catalog[-1] if i%2 else None for i in range(len(rows))]):
 header,materials=parse(run(overrides));assert header[:2]==base_header[:2]
 for i,((actual,aux),(original,old_aux)) in enumerate(zip(materials,base)):
  expected=bytearray(original)
  if overrides[i] is not None:struct.pack_into('<I',expected,16,[n.lower() for n in catalog].index(overrides[i].lower()))
  assert actual==expected and aux==old_aux,(i,actual.hex(),expected.hex())
 assert run(overrides,header[3]).stdout==run(overrides).stdout
 short=run(overrides,header[3]-1);assert short.returncode==1 and short.stdout.strip()==b'-4'
 cases+=1
# A new texture must append after base textures, not alter base alpha flags.
extra=next(e['name'] for a in inv if a['path']=='maps1.vpp' for e in a['vpp']['entries']
           if e['name'].lower().endswith('.tga') and len(e['name'])<=60 and e['name'].lower() not in [n.lower() for n in catalog])
overrides=[extra.upper() if i%2 else extra for i in range(len(rows))]
header,materials=parse(run(overrides));assert header[1]==base_header[1]+1
for (actual,aux),(original,old_aux) in zip(materials,base):
 expected=bytearray(original);struct.pack_into('<I',expected,16,base_header[1])
 assert actual==expected and aux==old_aux
assert run(overrides,header[3]).stdout==run(overrides).stdout
assert run(overrides,header[3]-1).stdout.strip()==b'-4'
for name,status in [('missing_rf_skin.tga',b'-3'),('x'*61,b'-4')]:
 failed=run([name]*len(rows));assert failed.returncode==1 and failed.stdout.strip()==status,failed.stdout
report=dict(result='PASS',cases=cases+1,materials=len(rows),base_textures=len(catalog),extra_texture=extra,
 scope='Actual PC archive loading, all200 runtime material bytes and array sentinel preserved except selected primary handle; null overrides, existing/case-folded/new duplicate names, exact/short budgets, missing/overlong errors, poisoned/freed input storage and repeat close. No native XEMU or glare ownership claim.')
(root/'artifacts/material-overrides.json').write_text(json.dumps(report,indent=2));print(report)
