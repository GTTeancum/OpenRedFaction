"""Decode every model-referenced texture through the shared named loader."""
import io,json,subprocess
from pathlib import Path
from PIL import Image
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];inventory=json.loads((root/'artifacts/inventory.json').read_text())
names=set();entries={};archives=[]
for archive in inventory['files']:
    if not archive.get('vpp'):continue
    index=len(archives);archives.append(root/'Installed_Game'/archive['path'])
    for entry in archive['vpp']['entries']:
        entries.setdefault(entry['name'].lower(),(index,entry))
        if not entry['name'].lower().endswith('.v3c'):continue
        with archives[-1].open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        for s in inspect(raw)['sections']:
            if s['type']!='0x5355424d':continue
            for i in range(s['materials']):
                record=raw[s['material_offset']+i*84:s['material_offset']+(i+1)*84]
                for offset in (0,48):
                    name=record[offset:offset+32].split(b'\0')[0].decode('ascii').lower()
                    if name:names.add(name)
# Only archives containing referenced names, retaining inventory order.
selected=sorted({entries[n][0] for n in names});assert len(selected)<=16
args=[str(root/'build/pc/Release/rf_material_probe.exe'),'--named',str(64*1024*1024),*[str(archives[i]) for i in selected]]
ordered=sorted(names);payload='\n'.join(ordered)+'\n'
run=subprocess.run(args,input=payload,text=True,capture_output=True);assert run.returncode==0,run.stdout+run.stderr
lines=run.stdout.splitlines();header=list(map(int,lines[0].split()));assert header[:3]==[len(names),len(names),0]
for name,line in zip(ordered,lines[1:],strict=True):
    index,entry=entries[name]
    with archives[index].open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
    image=Image.open(io.BytesIO(raw)).convert('RGBA')
    if raw[16]==32 and not(raw[17]&15):image.putalpha(255)
    value=2166136261
    for byte in image.tobytes():value=((value^byte)*16777619)&0xffffffff
    assert line==f'{name} 0 {selected.index(index)} {image.width} {image.height} {value}',name
low=args.copy();low[2]='1';failure=subprocess.run(low,input=payload,text=True,capture_output=True)
assert failure.returncode==1 and failure.stdout.strip()=='-4'
report=dict(result='PASS',textures=len(names),accounted_bytes=header[3],archives=len(selected),budget_rejections=1,scope='All model-referenced TGA images decoded via named loader; dimensions and RGBA hashes match Pillow; no model rendering or original texture-format classification')
(root/'artifacts/model-textures-verification.json').write_text(json.dumps(report,indent=2));print(report)
