"""Extract a candidate Holey01 template from the bounded original Ghidra export.

This validates topology/numbers, not original calling conventions or runtime parity.
Raw asset output stays under artifacts; game inputs remain read-only.
"""
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
SHA='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def run():
    assert hashlib.sha256((ROOT/'Installed_Game/RF.exe').read_bytes()).hexdigest()==SHA
    source=ROOT/'artifacts/analysis/rf_b8fb9ab4c9bf/4e6d60.c.txt'
    text=source.read_text();assert SHA in text
    text=text[:text.index('s_bit_driller_double')]
    words=[list(map(lambda x:int(x,16),m)) for m in re.findall(r'FUN_00409fe0\(local_\w+,(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\)',text)]
    f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
    vertices=[[f(w) for w in row] for row in words];assert len(vertices)==10
    faces=[];uv=[]
    for block in text.split('FUN_004cfab0(local_3c);')[1:]:
        rows=re.findall(r'uVar6 = (0x[0-9a-f]+);\s*uVar3 = (0x[0-9a-f]+);\s*puVar4 = .*?FUN_0040a480\((\d+)\);\s*FUN_004e0140',block)
        assert len(rows)==3
        faces.append([int(row[2]) for row in rows]);uv.append([[int(row[1],16),int(row[0],16)] for row in rows])
    assert len(faces)==16
    cross=lambda a,b:[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    dot=lambda a,b:sum(x*y for x,y in zip(a,b))
    sub=lambda a,b:[x-y for x,y in zip(a,b)]
    edges={};volume=0;inward_worst=0;violations=[];origin_distances=[]
    for face_id,face in enumerate(faces):
        assert len(set(face))==3 and all(0<=i<10 for i in face)
        a,b,c=[vertices[i] for i in face];n=cross(sub(b,a),sub(c,a));length=math.sqrt(dot(n,n));assert length>0
        origin_distances.append(-dot(n,a)/length)
        for vertex_id,p in enumerate(vertices):
            distance=-dot(n,sub(p,a))/length
            if distance>1e-6:violations.append(dict(face=face_id,vertex=vertex_id,distance=distance))
        inward_worst=max(inward_worst,max(-dot(n,sub(p,a))/length for p in vertices))
        volume+=dot(a,cross(b,c))/6
        for i,j in zip(face,face[1:]+face[:1]):edges.setdefault(tuple(sorted((i,j))),[]).append((i,j))
    assert len(edges)==24 and 10-len(edges)+len(faces)==2
    assert all(len(e)==2 and e[0]==e[1][::-1] for e in edges.values())
    assert volume<0, volume
    out=dict(scope='candidate data extracted from decompiler; runtime construction still to verify',exe_sha256=SHA,
        export_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),entry='004e6d60',name='Holey01.v3d',
        vertex_words=words,vertices=vertices,faces=faces,uv_words=uv,edges=len(edges),signed_volume=volume,
        origin_inside_all_face_planes=min(origin_distances)>0, convexity_violations=violations,
        winding="inward", convex=inward_worst<1e-6, maximum_outside_inward_plane_distance=inward_worst)
    path=ROOT/'artifacts/geomod-holey01.json';path.write_text(json.dumps(out,indent=2)+'\n')
    print('PASS: ten vertices, sixteen faces, twenty-four paired edges; signed volume',volume, 'convex', inward_worst<1e-6)
if __name__=='__main__':run()
