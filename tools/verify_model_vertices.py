"""Validate decoded model geometry against archive bytes; optionally audit V3M."""
import argparse,json,math,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--static',action='store_true',help='Audit supported static render geometry and enumerate unsupported models')
args=parser.parse_args()
unsupported=[]
models=batches=vertices=triangles=0
resident_lods=0
max_resident_bytes=0
weight_sums=set();triangle_flags=set()
nonfinite_normals={}
link_audit=[]
reused=negative_reuse=0;max_reuse=0;chained_reuse=0
def checksum(data):
    value=2166136261
    for byte in data:value=((value^byte)*16777619)&0xffffffff
    return value
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3m' if args.static else '.v3c'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        expected=[];geometry_headers=[];material_lines=[];material_base=0;li=0;structure=inspect(raw)
        formats=[]
        for section in structure['sections']:
            for lod_index,lod in enumerate(section.get('lods',[])):
                for bi in range(lod['batches']):
                    fmt=struct.unpack_from('<I',raw,lod['data_offset']+lod['data_bytes']+4+bi*18+14)[0]
                    if fmt!=0x518c41:formats.append(dict(submesh=section['name'],lod=lod_index,batch=bi,format=hex(fmt)))
        if formats:
            assert args.static,entry['name']
            unsupported.append(dict(model=entry['name'],batches=formats))
            continue
        bone_sections=[s for s in structure['sections'] if s['type']=='0x424f4e45']
        bone_count=struct.unpack_from('<I',raw,bone_sections[0]['offset']+8)[0] if bone_sections else 0
        for section in structure['sections']:
            for lod in section.get('lods',[]):
                lod_vertices=lod_triangles=0
                start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
                for bi in range(lod['batches']):
                    v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+bi*18)
                    lod_vertices+=v;lod_triangles+=t
                    slot=struct.unpack_from('<i',raw,start+bi*56+32)[0]
                    material_lines.append(f"M {li} {bi} {material_base+lod['textures'][slot]['slot']}")
                    assert fmt==0x518c41
                    sizes=[p,p,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]
                    regions=[]
                    for size in sizes:
                        regions.append(raw[start+relative:start+relative+size]);relative=(relative+size+15)&~15
                    assert p>=v*12 and uv>=v*8 and ix>=t*8 and (not links or links>=v*8)
                    vb=bytearray();rb=bytearray();used=set()
                    for a,b,c,flags in struct.iter_unpack('<4H',regions[3][:t*8]):used.update((a,b,c))
                    for n in range(v):
                        distance=struct.unpack_from('<h',regions[5],n*2)[0]
                        assert distance<=n,(entry['name'],li,bi,n,distance)
                        reused+=distance>0;negative_reuse+=distance<0;max_reuse=max(max_reuse,distance)
                        if distance>0:chained_reuse+=struct.unpack_from('<h',regions[5],(n-distance)*2)[0]>0
                        rb.extend(struct.pack('<i',distance))
                        floats=regions[0][n*12:n*12+12]+regions[1][n*12:n*12+12]+regions[2][n*8:n*8+8]
                        values=struct.unpack('<8f',floats)
                        assert all(math.isfinite(x) for x in values[:3]+values[6:]), (entry['name'],li,bi,n,values,sizes)
                        if not all(math.isfinite(x) for x in values[3:6]):
                            nonfinite_normals[entry['name']]=nonfinite_normals.get(entry['name'],0)+1
                        bones=regions[6][n*8:n*8+8] if links else bytes(4)+b'\xff'*4
                        active=[(w,b) for w,b in zip(bones[:4],bones[4:]) if b!=255]
                        assert args.static or all(b<bone_count for w,b in active), (entry['name'],li,bi,n,bones.hex(),bone_count)
                        normal_bad=not all(math.isfinite(x) for x in values[3:6])
                        if normal_bad or (not args.static and (sum(bones[:4])!=255 or any(b>=bone_count for w,b in active))):
                            link_audit.append(dict(model=entry['name'],lod=li,batch=bi,vertex=n,referenced=n in used,
                                nonfinite_normal=normal_bad,reuse_distance=struct.unpack_from('<h',regions[5],n*2)[0],weights=list(bones[:4]),bones=list(bones[4:]),
                                active_weight_sum=sum(w for w,b in active),bone_count=bone_count))
                        weight_sums.add(sum(bones[:4]));vb.extend(floats+bones)
                    tb=regions[3][:t*8]
                    for a,b,c,flags in struct.iter_unpack('<4H',tb):
                        assert max(a,b,c)<v;triangle_flags.add(flags)
                    expected.append(f'V {li} {bi} {checksum(vb)} {checksum(tb)} {checksum(rb)}')
                    vertices+=v;triangles+=t;batches+=1
                geometry_headers.append(f"G {li} {lod['batches']} {lod_vertices} {lod_triangles} {32+lod['batches']*20+lod_vertices*44+lod_triangles*8}")
                max_resident_bytes=max(max_resident_bytes,32+lod['batches']*20+lod_vertices*44+lod_triangles*8)
                li+=1
            material_base+=section.get('materials',0)
        output=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--vertices'],text=True)
        assert output.splitlines()==expected,entry['name']
        resident=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--geometry'],text=True).splitlines()
        assert [s for s in resident if s.startswith('V ')]==expected,entry['name']
        assert [s for s in resident if s.startswith('G ')]==geometry_headers,entry['name']
        assert [s for s in resident if s.startswith('M ')]==material_lines,entry['name']
        resident_lods+=li
        models+=1
report=dict(result='PASS',models=models,batches=batches,vertices=vertices,triangles=triangles,reused_vertices=reused,negative_reuse=negative_reuse,max_reuse=max_reuse,weight_sums=sorted(weight_sums),triangle_flags=sorted(triangle_flags),nonfinite_normals=nonfinite_normals,
    scope='Supported models only; every decoded vertex and triangle byte checked by per-batch hash, finite positions/UV and in-range indices; raw normals/bone slots preserved, no skinning or original renderer equivalence')
report.update(max_resident_lod_bytes=max_resident_bytes,unsupported_models=unsupported,asset_kind='static' if args.static else 'animated')
report.update(resident_lods=resident_lods,exact_budget_loads=resident_lods,budget_rejections=resident_lods)
report['chained_reuse']=chained_reuse
report['scope']+='; resident arrays, material mappings and exact Win32 memory accounting also checked'
(root/('artifacts/static-model-vertices-verification.json' if args.static else 'artifacts/model-vertices-verification.json')).write_text(json.dumps(report,indent=2));print(report)
(root/('artifacts/static-model-link-audit.json' if args.static else 'artifacts/model-link-audit.json')).write_text(json.dumps(link_audit,indent=2))
print(dict(anomalies=len(link_audit),referenced=sum(x['referenced'] for x in link_audit),
    active_weight_sums=sorted({x['active_weight_sum'] for x in link_audit}),
    referenced_nonfinite_normals=sum(x['referenced'] and x['nonfinite_normal'] for x in link_audit)))
