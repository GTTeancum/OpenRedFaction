"""Check streamed batch ranges against installed blobs and descriptor tables."""
import json, struct, subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=batches=lod_count=0
formats={};owned_lods=0;peak=0
def hash_bytes(raw):
    h=2166136261
    for byte in raw:h=((h^byte)*16777619)&0xffffffff
    return h
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith(('.v3c','.v3m')):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        expected=[];lod_index=0;material_base=0
        for section in inspect(raw)['sections']:
            for lod in section.get('lods',[]):
                if not lod['flags']&32:lod_index+=1;continue
                budget=20+lod['data_bytes']+lod['batches']*20;peak=max(peak,budget);owned_lods+=1
                expected.append(f'C {lod_index} {lod["batches"]} {budget}')
                start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
                for i in range(lod['batches']):
                    v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+i*18)
                    sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]
                    fields=[lod_index,i,v,t,fmt]
                    for size in sizes:
                        assert start+relative+size<=lod['attachment_offset']
                        fields.extend((start+relative if size else 0,size))
                        relative=(relative+size+15)&~15
                    vh=hash_bytes(raw[fields[5]:fields[5]+v*12]);ph=hash_bytes(raw[fields[13]:fields[13]+t*16]);th=hash_bytes(raw[fields[11]:fields[11]+t*8])
                    expected.append('D '+' '.join(map(str,(lod_index,i,fields[11],t,vh,ph,th))))
                    formats[str(fmt)]=formats.get(str(fmt),0)+1
                assert start+relative==lod['attachment_offset']
                lod_index+=1
            material_base+=section.get('materials',0)
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--collision-geometry'],text=True)
        assert [s for s in output.splitlines() if s.startswith(('C ','D '))]==expected,entry['name']
        models+=1;batches+=sum(line.startswith('D ') for line in expected);lod_count+=lod_index
report=dict(result='PASS',models=models,lods=lod_count,batches=batches,owned_lods=owned_lods,peak_owned_bytes=peak,formats=formats,
    scope='All installed static LODs: exact owned positions/planes/records and file-offset tokens, budget-minus-one rejection, exact-budget loading and repeated cleanup; animated missing-plane rejection. PC file I/O; native build only, no XEMU or scene binding.')
(root/'artifacts/model-collision-geometry.json').write_text(json.dumps(report,indent=2));print(report)
