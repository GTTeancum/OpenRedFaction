"""Compare shared portal records with independently walked installed payloads."""
import json,struct,subprocess,math
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
for level in levels:
    section=next(s for s in level['sections'] if s['type']=='0x100')
    archive=next(a for a in inventory['files'] if a['path']==level['archive'])
    entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
    with (root/'Installed_Game'/level['archive']).open('rb') as f:
        f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
    at=6
    def number():
        global at
        n=struct.unpack_from('<I',data,at)[0];at+=4;return n
    def string():
        global at
        n=struct.unpack_from('<H',data,at)[0];at+=2+n
    for _ in range(number()):string()
    n=number();at+=n*12;rooms=number()
    for _ in range(rooms):
        raw=data[at:at+40];at+=40;string()
        if raw[32]:at+=8;string();at+=37
        if raw[33]:at+=4
    for _ in range(number()):number();n=number();at+=n*4
    count=number();records=data[at:at+count*32]
    output=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),
        str(root/'Installed_Game'/level['archive']),level['file'],'--portals'],text=True).splitlines()
    assert tuple(map(int,output[0].split()))==(rooms,count)
    shared=b''.join(struct.pack('<8I',*(int(w,16) for w in line.split())) for line in output[1:])
    assert shared==records
    adjacency=[[] for _ in range(rooms)];bounds=[]
    for i,(a,b,*vectors) in enumerate(struct.iter_unpack('<2I6f',records)):
        assert a<rooms and b<rooms and all(math.isfinite(v) for v in vectors)
        assert all(vectors[j]<=vectors[j+3] for j in range(3))
        adjacency[a].append(i);adjacency[b].append(i);bounds.append(dict(rooms=[a,b],minimum=vectors[:3],maximum=vectors[3:]))
    results.append(dict(level=level['file'],rooms=rooms,portals=count,records=bounds,adjacency=adjacency))
report=dict(result='PASS',levels=len(results),portals=sum(r['portals'] for r in results),
    scope='Independent v180 payload walk versus PC shared portal accessor, endpoints and six float bits. Probe checks insufficient-capacity preservation. All installed bounds ordered/finite; adjacency derived from documented original append rule, not original constructor execution. Native NXDK/runtime binding excluded.')
(root/'artifacts/geometry-portals-verification.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
