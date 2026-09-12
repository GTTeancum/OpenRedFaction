"""Compare retained material rows with file-backed loading and measure opening textures."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game';probe=root/'build/pc/Release/rf_material_probe.exe'
archives=[str(game/n) for n in ('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')]
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];entries={e['name'].lower():e for a in inv if a['path']=='meshes.vpp' for e in a['vpp']['entries']}
names=[r['model'] for r in json.loads((root/'artifacts/static-render-resource-nxdk.json').read_text())['records']]
combined=bytearray();results=[]
def records(raw,budget):
 return subprocess.run([str(probe),'--records',str(budget),*archives],input=struct.pack('<I',len(raw)//84)+raw,capture_output=True)
for name in names+[next(e['name'] for e in entries.values() if e['name'].lower().endswith('.v3c'))]:
 e=entries[name.lower()]
 with (game/'meshes.vpp').open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
 raw=b''.join(data[s['material_offset']:s['material_offset']+s['materials']*84] for s in inspect(data)['sections'] if s['type']=='0x5355424d')
 if name in names:combined.extend(raw)
 old=subprocess.run([str(probe),'--model',str(game/'meshes.vpp'),name,'4194304',*archives],capture_output=True)
 new=records(raw,4194304);assert (new.returncode,new.stdout)==(old.returncode,old.stdout),(name,new.stdout,old.stdout)
 assert new.returncode==0,(name,new.stdout)
 header=list(map(int,new.stdout.splitlines()[0].split()));peak=header[3]
 assert records(raw,peak).stdout==new.stdout,name
 short=records(raw,peak-1);assert short.returncode==1 and short.stdout.strip()==b'-4',(name,short.stdout)
 results.append(dict(model=name,materials=header[0],textures=header[1],resident=header[2],peak=peak))
r=records(bytes(combined),4194304);assert r.returncode==0,r.stdout
header=list(map(int,r.stdout.splitlines()[0].split()));assert records(bytes(combined),header[3]).stdout==r.stdout
assert records(bytes(combined),header[3]-1).stdout.strip()==b'-4'
# Invalid primary/secondary names and unavailable texture must clean partial ownership.
for offset,data,status in [(0,bytes(32),b'-2'),(48,b'x'*32,b'-2'),(0,b'no_such_rf_texture.tga\0'.ljust(32,b'\0'),b'-3')]:
 raw=bytearray(combined[:84]);raw[offset:offset+32]=data;bad=records(bytes(raw),4194304)
 assert bad.returncode==1 and bad.stdout.strip()==status,bad.stdout
report=dict(result='PASS',models=len(results),guards=3,opening_materials=header[0],opening_textures=header[1],opening_resident=header[2],opening_peak=header[3],scope='PC actual archive image loading: retained rows equal file-backed runtime material bytes/handles/array sentinel and accounting; exact/short budgets, source rows poisoned/freed before output, repeated close and malformed/missing texture errors. Texture pixel hashes and native XEMU not independently checked here.',records=results)
(root/'artifacts/material-records.json').write_text(json.dumps(report,indent=2));print(report)
