"""Check Live Mines UID/class/skin bindings against separately verified inputs."""
import json,struct,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
table=json.loads((root/'artifacts/entity-assets-verification.json').read_text())
classes={a['name'].lower():a for a in table['assets']}
inventory=json.loads((root/'artifacts/inventory.json').read_text())
meshes=next(a for a in inventory['files'] if a['path']=='meshes.vpp')
entries={e['name'].lower():e for e in meshes['vpp']['entries']}
probe=root/'build/pc/Release/rf_entity_assets_probe.exe'
args=[str(probe),'--level',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl',str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/meshes.vpp')]
raw=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),args[2],args[3]])
assert len(raw)%1084==0
results=[]
for offset in range(0,len(raw),1084):
    record=raw[offset:offset+1084];uid=struct.unpack_from('<i',record)[0]
    name=record[52:308].split(b'\0')[0].decode('cp1252');skin=record[820:1076].split(b'\0')[0].decode('cp1252')
    definition=classes.get(name.lower());textures=[];expected=0;mesh=None
    if definition is None:expected=-3
    else:
        skins={k.lower():v for k,v in definition['skins'].items()}
        if skin and skin.lower() not in skins:expected=-3
        else:
            textures=skins[skin.lower()] if skin else []
            model=definition['model']
            if not model.lower().endswith('.vcm'):expected=-2
            else:
                mesh=entries.get((model.rsplit('.',1)[0]+'.v3c').lower())
                if mesh is None:expected=-3
    run=subprocess.run(args+[str(uid)],capture_output=True,text=True)
    lines=run.stdout.splitlines()
    if expected:
        assert run.returncode==3 and lines==[str(expected)],(uid,name,run.stdout,run.stderr)
    else:
        assert run.returncode==0,(uid,name,run.stdout,run.stderr)
        assert lines[0]==f'{uid} {name} {skin} {mesh["name"]}',(uid,lines)
        transform=[float(x) for line in lines[1:3] for x in line.split()]
        assert struct.pack('<12f',*transform)==record[4:52],uid
        assert lines[3:]==textures,(uid,lines[3:],textures)
    results.append(dict(uid=uid,name=name,skin=skin,status=expected,mesh=mesh['name'] if mesh else None))
absent=subprocess.run(args+['-2147483648'],capture_output=True,text=True)
assert absent.returncode==3 and absent.stdout.strip()=='-3'
guards=[('$Name: "other" $V3D Filename: "miner.vcm"',-3),
        ('$Name: "miner1" $V3D Filename: "camera1.v3d"',-2),
        ('$Name: "miner1" $V3D Filename: ""',-2),
        ('$Name: "miner1" $V3D Filename: "missing_actor_mesh.vcm"',-3)]
with tempfile.TemporaryDirectory(dir=root/'artifacts') as directory:
    path=Path(directory)/'tables.vpp'
    for declaration,status in guards:
        payload=declaration.encode();image=bytearray(6144)
        struct.pack_into('<4I',image,0,0x51890ace,1,1,len(image))
        image[2048:2059]=b'entity.tbl\0';struct.pack_into('<I',image,2108,len(payload))
        image[4096:4096+len(payload)]=payload;path.write_bytes(image)
        variant=args.copy();variant[4]=str(path)
        rejected=subprocess.run(variant+['9858'],capture_output=True,text=True)
        assert rejected.returncode==3 and rejected.stdout.strip()==str(status),(declaration,rejected.stdout)
report=dict(result='PASS',entities=len(results),resolved=sum(r['status']==0 for r in results),
    binding_failure_guards=len(guards)+1,
    unresolved=[r for r in results if r['status']],details=results,
    scope='L1S1 bindings composed from independently verified entity records, table declarations and VPP inventory; exact transforms and ordered skins; failure output preservation checked by probe. No actor spawning, animation-state or original gameplay-loader equivalence claim.')
(root/'artifacts/level-actor-assets.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='details'})
