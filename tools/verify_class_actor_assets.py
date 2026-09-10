"""Check class-only skeletal binding against verified tables and archive inventory."""
import hashlib
import json
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
definitions=json.loads((ROOT/'artifacts/entity-assets-verification.json').read_text())['assets']
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
entries={e['name'].lower():e for f in inventory['files'] if f['path']=='meshes.vpp' for e in f['vpp']['entries']}
probe=ROOT/'build/pc/Release/rf_entity_assets_probe.exe'
args=[str(probe),'--class',str(ROOT/'Installed_Game/tables.vpp'),str(ROOT/'Installed_Game/meshes.vpp')]
reports=[]
for definition in definitions:
    for skin,textures in [('',[])]+list(definition['skins'].items()):
        model=definition['model']
        mesh=entries.get((model.rsplit('.',1)[0]+'.v3c').lower()) if model.lower().endswith('.vcm') else None
        status=-2 if not model.lower().endswith('.vcm') else (-3 if mesh is None else 0)
        run=subprocess.run(args+[definition['name'],skin],capture_output=True,text=True)
        if status:
            assert run.returncode==3 and run.stdout.strip()==str(status),(definition['name'],skin,run.stdout,run.stderr)
        else:
            assert run.returncode==0,(definition['name'],skin,run.stdout,run.stderr)
            assert run.stdout.splitlines()==[f'{mesh["name"]} {mesh["offset"]} {mesh["size"]}',model]+textures
        reports.append(dict(name=definition['name'],skin=skin,status=status))

guards=[('$Name: "other" $V3D Filename: "miner.vcm"',-3),
        ('$Name: "miner1" $V3D Filename: "camera1.v3d"',-2),
        ('$Name: "miner1" $V3D Filename: ""',-2),
        ('$Name: "miner1" $V3D Filename: "missing_actor_mesh.vcm"',-3)]
with tempfile.TemporaryDirectory(dir=ROOT/'artifacts') as directory:
    path=Path(directory)/'tables.vpp'
    for declaration,status in guards:
        payload=declaration.encode();image=bytearray(6144)
        struct.pack_into('<4I',image,0,0x51890ace,1,1,len(image))
        image[2048:2059]=b'entity.tbl\0';struct.pack_into('<I',image,2108,len(payload))
        image[4096:4096+len(payload)]=payload;path.write_bytes(image)
        variant=args.copy();variant[2]=str(path)
        run=subprocess.run(variant+['miner1',''],capture_output=True,text=True)
        assert run.returncode==3 and run.stdout.strip()==str(status),(declaration,run.stdout,run.stderr)
report=dict(result='PASS',selections=reports,failure_guards=len(guards),
    probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
    scope='Port class/skin/archive composition, independent of level records. All authored selections checked against separately verified table metadata and VPP inventory; failure preservation checked by probe. No original full-loader or player spawn equivalence.')
(ROOT/'artifacts/class-actor-assets.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS: {len(reports)} class/skin selections; {len(guards)} failure-preservation guards')
