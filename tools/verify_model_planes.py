"""Check streamed batch ranges against installed blobs and descriptor tables."""
import json, struct, subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=batches=lod_count=0
formats={}
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith(('.v3c','.v3m')):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        expected=[];lod_index=0;material_base=0
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
                    plane_start=fields[13];plane_size=fields[14];h=2166136261
                    for byte in raw[plane_start:plane_start+plane_size]:h=((h^byte)*16777619)&0xffffffff
                    expected.append('P '+ ' '.join(map(str,(lod_index,i,plane_size//16,h))))
                    formats[str(fmt)]=formats.get(str(fmt),0)+1
                assert start+relative==lod['attachment_offset']
                lod_index+=1
            material_base+=section.get('materials',0)
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--planes'],text=True)
        assert [s for s in output.splitlines() if s.startswith('P ')]==expected,entry['name']
        models+=1;batches+=len(expected);lod_count+=lod_index
report=dict(result='PASS',models=models,lods=lod_count,batches=batches,index_rejections=batches,formats=formats,
    scope='All installed V3C/V3M structural envelopes and stored plane byte hashes, missing-plane behavior and preserved invalid-index/truncated-region outputs. PC file I/O; no native XEMU or live geometry ownership.')
(root/'artifacts/model-planes-verification.json').write_text(json.dumps(report,indent=2));print(report)
