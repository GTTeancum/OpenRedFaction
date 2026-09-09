"""Exercise every installed resident batch with a controlled rendering fixture."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=batches=vertices=reused=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3c'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        expected=[];li=0
        for section in inspect(raw)['sections']:
            for lod in section.get('lods',[]):
                start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
                for bi in range(lod['batches']):
                    v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+bi*18)
                    sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]
                    regions=[]
                    for size in sizes:regions.append(start+relative);relative=(relative+size+15)&~15
                    duplicates=sum(struct.unpack_from('<h',raw,regions[5]+n*2)[0]>0 for n in range(v))
                    expected.append(f'R {li} {bi} {v} {v-duplicates} {duplicates}')
                    vertices+=v;reused+=duplicates
                li+=1
        actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--render-geometry'],text=True).splitlines()
        assert actual==expected,entry['name']
        models+=1;batches+=len(expected)
report=dict(result='PASS',models=models,batches=batches,vertices=vertices,reused=reused,
    scope='All installed resident batches through assembled processing with identity bone matrices, fixed camera and ambient-only lighting; independent vertex/reuse counts, UV/color/preserved-field and reuse-copy checks; not original pose or full rendered-scene parity')
(root/'artifacts/model-render-files-verification.json').write_text(json.dumps(report,indent=2));print(report)
