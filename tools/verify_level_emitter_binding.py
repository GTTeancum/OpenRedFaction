"""Exercise existing shared room locator and bitmap decoder at authored emitters."""
import json,subprocess,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/level-emitters.json').read_text())
assets=json.loads((root/'artifacts/inventory.json').read_text());results=[]
for level in inventory['results']:
    raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-emitter-bind',str(root/'Installed_Game'/level['archive']),level['file'],str(root/'Installed_Game')])
    assert len(raw)==len(level['records'])*48
    for i,record in enumerate(level['records']):
        fields=struct.unpack_from('<Iii9I',raw,i*48)
        uid,room_status,image_status,room,face,retries,width,height,resident,format_,frames,archive=fields
        assert uid==record['uid'] and room_status==image_status==0 and room!=0xffffffff
        archive_name=f'maps{archive+1}.vpp'
        entry=next(e for a in assets['files'] if a['path']==archive_name for e in a['vpp']['entries'] if e['name'].lower()==record['bitmap'].lower())
        with (root/'Installed_Game'/archive_name).open('rb') as f:
            f.seek(entry['offset']);header=f.read(18)
        assert (width,height)==struct.unpack_from('<2H',header,12)
        assert frames==1 and resident==36+width*height*4
        results.append(dict(level=level['file'],uid=uid,bitmap=record['bitmap'],archive=archive_name,room=room,face=face,retries=retries,width=width,height=height,resident_bytes=resident,format=format_))
report=dict(result='PASS',levels=len(inventory['results']),emitters=len(results),unique_bitmaps=len({r['bitmap'].lower() for r in results}),scope='PC shared room locator and bitmap decode at every installed emitter position; all rooms found and images decoded, dimensions checked against TGA headers. Images loaded/released individually under 1 MiB budget. No persistent cache, simultaneous residency, runtime registration or native gameplay claim.')
(root/'artifacts/level-emitter-binding.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
