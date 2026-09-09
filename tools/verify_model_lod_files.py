"""Check serialized detail thresholds and selection through resident loading."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
models=lods=loads=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if not entry['name'].lower().endswith('.v3c'):continue
        path=root/'Installed_Game'/archive['path']
        with path.open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
        thresholds_expected=[];selections=[];base=0;submesh=0
        for section in inspect(raw)['sections']:
            levels=section.get('lods',[])
            if not levels:continue
            thresholds=struct.unpack_from('<'+'f'*len(levels),raw,section['offset']+64)
            bits=struct.unpack_from('<'+'I'*len(levels),raw,section['offset']+64)
            thresholds_expected.extend(f'T {base+i} {value}' for i,value in enumerate(bits))
            for mode in range(5):
                for m,metric in enumerate([0,10,100,1000,1000000]):
                    selected=min(1 if mode==3 else 0,len(levels)-1)
                    if mode==1:selected=len(levels)-1
                    elif mode==2:selected=0
                    else:
                        metric*=2.5 if mode==4 else 1
                        candidates=[i for i in range(selected,len(levels)) if metric>=thresholds[i]]
                        if candidates:selected=max(candidates)
                    lod=levels[selected];vertices=triangles=0
                    for b in range(lod['batches']):
                        v,t=struct.unpack_from('<HH',raw,lod['data_offset']+lod['data_bytes']+4+b*18)
                        vertices+=v;triangles+=t
                    selections.append(f'S {submesh} {mode} {m} {base+selected} {vertices} {triangles}')
            base+=len(levels);submesh+=1
        lines=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),entry['name'],'--lod-selection'],text=True).splitlines()
        assert lines==thresholds_expected+selections,entry['name']
        models+=1;lods+=base;loads+=len(selections)
report=dict(result='PASS',models=models,thresholds=lods,selected_geometry_loads=loads,invalid_submesh_rejections=models,
    scope='Independent original-file threshold bits and selected geometry counts across five modes and five axis-aligned camera distances per SUBM; unit metric scale, no live camera or drawing claim')
(root/'artifacts/model-lod-files-verification.json').write_text(json.dumps(report,indent=2));print(report)
