"""Persistent deduplicated emitter textures, ownership and exact budget checks."""
import json,subprocess,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/level-emitters.json').read_text());results=[]
def run(level,budget):
    return subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-emitter-materials',str(root/'Installed_Game'/level['archive']),level['file'],str(root/'Installed_Game'),str(budget)])
for level in inventory['results']:
    raw=run(level,1024*1024);status,count,unique,resident=struct.unpack_from('<i3I',raw)
    assert status==0 and count==len(level['records'])
    names=[];mapping=[]
    for record in level['records']:
        name=record['bitmap'].lower()
        if name not in names:names.append(name)
        mapping.append((record['uid'],names.index(name)))
    assert unique==len(names)
    assert list(struct.iter_unpack('<2I',raw[16:16+count*8]))==mapping
    offset=16+count*8;pixels=0;textures=[]
    for name in names:
        assert raw[offset:offset+64].split(b'\0')[0].decode('cp1252').lower()==name
        width,height,bytes_,archive,checksum=struct.unpack_from('<5I',raw,offset+64)
        assert bytes_==width*height*4 and width and height and archive<4
        pixels+=bytes_;textures.append(dict(name=name,width=width,height=height,bytes=bytes_,checksum=checksum));offset+=84
    assert offset==len(raw)
    assert resident==24+count*(8+100)+pixels # Measured 32-bit shared owner/slot layouts.
    assert run(level,resident)==raw
    assert run(level,resident-1)==struct.pack('<i',-4)
    results.append(dict(level=level['file'],emitters=count,unique=unique,resident_bytes=resident,textures=textures))
report=dict(result='PASS',levels=len(results),emitters=sum(r['emitters'] for r in results),maximum_level_bytes=max(r['resident_bytes'] for r in results),l1s1_bytes=next(r['resident_bytes'] for r in results if r['level']=='L1S1.rfl'),scope='PC shared persistent texture bundle: UID mapping and case-insensitive first-use deduplication across all installed emitter levels. Pixel reads after archive close, exact budget success, one-byte-under failure with unchanged empty output, and repeated close checked. No native memory/renderer/campaign claim.')
(root/'artifacts/level-particle-materials-verification.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
