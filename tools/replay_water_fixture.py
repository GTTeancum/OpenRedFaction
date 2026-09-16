"""Generate an authored dm03 wet-room pose and ordinary-input recording; no game launch.

Needs the frontend wet-fixture hook described in WATER-FIXTURE-DM03-20260916.md.
Runs existing read-only reconstructed collision probes, never the original game.
"""
import hashlib,json,struct,subprocess
from pathlib import Path
from inspect_geometry import inspect
ROOT=Path(__file__).resolve().parents[1]

def main():
    folder=ROOT/'artifacts/water-fixture';folder.mkdir(parents=True,exist_ok=True)
    inv=json.loads((ROOT/'artifacts/inventory.json').read_text())
    level=next(v for v in json.loads((ROOT/'artifacts/levels.json').read_text()) if v['file']=='dm03.rfl')
    section=next(v for v in level['sections'] if v['type']=='0x100')
    archive=next(v for v in inv['files'] if v['path']=='levelsm.vpp')
    entry=next(v for v in archive['vpp']['entries'] if v['name']=='dm03.rfl')
    path=ROOT/'Installed_Game/levelsm.vpp'
    with path.open('rb') as stream:
        stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
    layout=inspect(data);vo=layout['vertices_offset']
    vertices=[struct.unpack_from('<3f',data,vo+i*12) for i in range(layout['vertices'])]
    at=vo+layout['vertices']*12+4;faces=[]
    for index in range(layout['faces']):
        raw=data[at:at+56];at+=56;mapping=struct.unpack_from('<I',raw,20)[0]
        room,count=struct.unpack_from('<II',raw,48);stride=12 if mapping==0xffffffff else 20
        points=[vertices[struct.unpack_from('<I',data,at+j*stride)[0]] for j in range(count)];at+=count*stride
        faces.append(dict(index=index,room=room,flags=struct.unpack_from('<I',raw,40)[0],plane=struct.unpack_from('<4f',raw),points=points))
    water=faces[1737];assert water['room']==18 and water['flags']==260 and water['plane'][1]==1
    start=(-226.5,-38.25,-80);delta=(3.,-4.,0.)
    ray_lines=[(*start,*delta,4),(*start,0,-4,0,4),(*start,0,4,0,4)]
    probe=ROOT/'build/pc/Release/rf_collision_probe.exe'
    text=''.join(' '.join(map(str,row))+'\n' for row in ray_lines)
    output=subprocess.check_output([str(probe),'--world-ray-at',str(path),'dm03.rfl'],input=text,text=True)
    rows=[row.split() for row in output.splitlines()]
    assert [int(row[2]) for row in rows]==[491,498,500],output
    assert all(row[:2]==['0','1'] for row in rows)
    assert abs(float(rows[0][4])-.4375)<1e-6
    # Collapse only consecutive duplicate authored corners, preserving polygon shape.
    points=[]
    for point in water['points']:
        if not points or point!=points[-1]:points.append(point)
    if points[0]==points[-1]:points.pop()
    assert len(points)<=8
    lo=[min(p[k] for p in points) for k in range(3)];hi=[max(p[k] for p in points) for k in range(3)]
    padded=[x for p in points for x in p]+[0.]*(24-3*len(points));water_rows=[]
    for radius in [0.,.1,.125,.25]:
        wire=struct.pack('<41fIIiIIII',*water['plane'],*lo,*hi,*padded,*start,*delta,1,0x1004,260,-1,0,0,0,len(points))+struct.pack('<4f',*delta,radius)
        result=subprocess.check_output([str(probe),'--sweep'],input=wire)
        status,matched=struct.unpack_from('<iI',result);hit=struct.unpack_from('<7f',result,8)
        assert status==0 and matched and hit[0]<.4375,(radius,status,matched,hit)
        water_rows.append(dict(radius=radius,fraction=hit[0],position=hit[1:4],normal=hit[4:]))
    # Four ordinary next-weapon presses select slot4 from the default handgun.
    frames=180
    recording=b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==60),0,int(i in [10,20,30,40]),0) for i in range(frames))
    (folder/'input.bin').write_bytes(recording)
    report=dict(level='dm03.rfl',archive='levelsm.vpp',room=18,room_name='sewerpipe',layout=layout,
        geometry_sha256=hashlib.sha256(data).hexdigest(),camera_position=start,
        camera_orientation=[[0,0,-1],[.8,.6,0],[.6,-.8,0]],forward=[.6,-.8,0],
        liquid_face=water,solid_ray_evidence=dict(inputs=ray_lines,output=output),liquid_sphere_queries=water_rows,
        recording=dict(frames=frames,fire_frame=60,next_weapon_frames=[10,20,30,40],sha256=hashlib.sha256(recording).hexdigest()),
        scope='Authored geometry plus existing shared collision probes; no reconstructed runtime launch, scene staging, ripple visibility or character immersion claim')
    (folder/'fixture.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Generated water-fixture/input.bin and fixture.json; collision route confirms water before floor. Frontend hook still required.')
if __name__=='__main__':main()
