"""Check streamed batch ranges against installed blobs and descriptor tables."""
import json, struct, subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=batches=lod_count=0
formats={}
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3c'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        expected=[];lod_index=0
        for section in inspect(raw)['sections']:
            for lod in section.get('lods',[]):
                start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
                for i in range(lod['batches']):
                    v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+i*18)
                    sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]
                    fields=[lod_index,i,v,t,fmt]
                    for size in sizes:
                        assert start+relative+size<=lod['attachment_offset']
                        fields.extend((start+relative if size else 0,size))
                        relative=(relative+size+15)&~15
                    expected.append('B '+' '.join(map(str,fields)))
                    formats[str(fmt)]=formats.get(str(fmt),0)+1
                assert start+relative==lod['attachment_offset']
                lod_index+=1
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--batches'],text=True)
        assert [s for s in output.splitlines() if s.startswith('B ')]==expected,entry['name']
        models+=1;batches+=len(expected);lod_count+=lod_index
report=dict(result='PASS',models=models,lods=lod_count,batches=batches,bounds_rejections=lod_count,formats=formats,
            scope='All installed batch counts, format bits and eight aligned file ranges; numeric encodings, bone weights and rendering not yet verified')
(root/'artifacts/model-batches-verification.json').write_text(json.dumps(report,indent=2));print(report)
