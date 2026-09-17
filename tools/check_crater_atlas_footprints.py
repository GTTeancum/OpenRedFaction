"""Check live generated-face bilinear footprints stay inside their owned atlas tiles."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import struct


def verify(atlas,physical):
    with atlas.open() as source:maps=list(csv.DictReader(source))
    raw=physical.read_bytes();assert raw[:4]==b'RGM1'
    nv,nf=struct.unpack_from('<2I',raw,4);assert len(raw)==12+nv*20+nf*16
    faces=[struct.unpack_from('<4I',raw,12+nv*20+i*16) for i in range(nf)]
    generated=[f for f in faces if f[3]==0xffffffff];assert generated
    assert sum(int(row['faces']) for row in maps)==len(generated)
    assert sum(int(row['corners']) for row in maps)==sum(f[1] for f in generated)
    rectangles=[];failures=[];margin=math.inf
    for row in maps:
        x,y,w,h=[int(row[k]) for k in ('atlas_x','atlas_y','width','height')]
        assert 0<=x<x+w<=512 and 0<=y<y+h<=512
        rect=(x,y,x+w,y+h)
        assert all(rect[2]<=r[0] or r[2]<=rect[0] or rect[3]<=r[1] or r[3]<=rect[1] for r in rectangles),'overlapping atlas allocations'
        rectangles.append(rect)
        if not int(row['corners']):continue
        for axis,start,size in (('u',x,w),('v',y,h)):
            lo=float(row['min_pixel_'+axis]);hi=float(row['max_pixel_'+axis])
            assert math.isfinite(lo) and math.isfinite(hi) and lo<=hi
            # Every interior point of a convex face lies between its affine
            # projected corner extrema. Check both bilinear taps, conservatively
            # including the second tap at an exact integer coordinate.
            if math.floor(lo)<start or math.floor(hi)+1>=start+size:
                failures.append(dict(map=int(row['map']),axis=axis,minimum=lo,maximum=hi,start=start,size=size))
            margin=min(margin,lo-start,start+size-1-hi)
    report=dict(result='FAIL' if failures else 'PASS',generated_faces=len(generated),corners=sum(f[1] for f in generated),
                allocated_maps=len(maps),used_maps=sum(int(r['faces'])>0 for r in maps),minimum_texel_margin=margin,failures=failures,
                atlas_sha256=hashlib.sha256(atlas.read_bytes()).hexdigest(),mesh_sha256=hashlib.sha256(raw).hexdigest(),
                scope='Actual projected corner extrema and disjoint allocations; affine convex-face interiors are bounded by these extrema. Does not establish original mapping ownership or GPU interpolation precision.')
    print(json.dumps(report,indent=2));assert not failures
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=Path);a=p.parse_args()
    report=verify(a.folder/'atlas.csv',a.folder/'physical.mesh')
    (a.folder/'atlas-footprints.json').write_text(json.dumps(report,indent=2)+'\n')
