"""Validate float vertices, raw bone slots and triangle indices in every V3C."""
import json,math,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=batches=vertices=triangles=0
weight_sums=set();triangle_flags=set()
nonfinite_normals={}
def checksum(data):
    value=2166136261
    for byte in data:value=((value^byte)*16777619)&0xffffffff
    return value
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
                    assert fmt==0x518c41
                    sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]
                    regions=[]
                    for size in sizes:
                        regions.append(raw[start+relative:start+relative+size]);relative=(relative+size+15)&~15
                    assert p>=v*12 and uv>=v*8 and ix>=t*8 and (not links or links>=v*8)
                    vb=bytearray()
                    for n in range(v):
                        floats=regions[0][n*12:n*12+12]+regions[1][n*12:n*12+12]+regions[2][n*8:n*8+8]
                        values=struct.unpack('<8f',floats)
                        assert all(math.isfinite(x) for x in values[:3]+values[6:]), (entry['name'],li,bi,n,values,sizes)
                        if not all(math.isfinite(x) for x in values[3:6]):
                            nonfinite_normals[entry['name']]=nonfinite_normals.get(entry['name'],0)+1
                        bones=regions[6][n*8:n*8+8] if links else bytes(4)+b'\xff'*4
                        weight_sums.add(sum(bones[:4]));vb.extend(floats+bones)
                    tb=regions[3][:t*8]
                    for a,b,c,flags in struct.iter_unpack('<4H',tb):
                        assert max(a,b,c)<v;triangle_flags.add(flags)
                    expected.append(f'V {li} {bi} {checksum(vb)} {checksum(tb)}')
                    vertices+=v;triangles+=t;batches+=1
                li+=1
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--vertices'],text=True)
        assert output.splitlines()==expected,entry['name']
        models+=1
report=dict(result='PASS',models=models,batches=batches,vertices=vertices,triangles=triangles,weight_sums=sorted(weight_sums),triangle_flags=sorted(triangle_flags),nonfinite_normals=nonfinite_normals,
    scope='Every installed vertex and triangle byte checked by per-batch hash, finite positions/UV and in-range indices; raw normals/bone slots preserved, no skinning or original renderer equivalence')
(root/'artifacts/model-vertices-verification.json').write_text(json.dumps(report,indent=2));print(report)
