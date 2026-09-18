"""Independently evaluate visible cap pixels from retained draw inputs.

This audits the PC final image, not Xbox command execution or retail appearance.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import struct
from PIL import Image


def audit(folder):
    data = (folder/'far-cap-draw.bin').read_bytes()
    assert data[:4] == b'RFM1'
    count, world, stride = struct.unpack_from('<3I', data, 4)
    assert stride == 56 and len(data) == 16+count*stride and count % 3 == 0
    vertices = [struct.unpack_from('<9fI3fI', data, 16+i*stride) for i in range(count)]
    base = (folder/'cap-material.bin').read_bytes()
    material, bw, bh, fmt, size = struct.unpack_from('<5I', base, 4)
    assert base[:4] == b'RFT1' and size == bw*bh*4 and len(base) == 24+size
    with (folder/'far-cap-lightmaps.csv').open(newline='') as source:
        rows = list(csv.DictReader(source))
    images = {int(row['image']) for row in rows}
    assert len(images) == 1 and all(int(row['material']) == material for row in rows)
    image = images.pop()
    atlas = {}
    for row in rows:
        x, y, w, h = (int(row[k]) for k in ('atlas_x','atlas_y','width','height'))
        packed = bytes.fromhex(row['packed'])
        assert len(packed) == w*h*2
        for v in range(h):
            for u in range(w):
                key = (x+u,y+v)
                assert key not in atlas
                pixel = struct.unpack_from('<H', packed, (v*w+u)*2)[0]
                atlas[key] = tuple(((pixel >> shift) & 31)/31 for shift in (10,5,0))

    def sample(u, v, light=False):
        width, height = (512,512) if light else (bw,bh)
        u, v = (min(1,max(0,u)),min(1,max(0,v))) if light else (u % 1,v % 1)
        sx, sy = u*width-.5, v*height-.5
        ix, iy = math.floor(sx), math.floor(sy)
        fx, fy = sx-ix, sy-iy
        rgb = [0.,0.,0.]
        for dx, dy, weight in ((0,0,(1-fx)*(1-fy)),(1,0,fx*(1-fy)),(0,1,(1-fx)*fy),(1,1,fx*fy)):
            x, y = ix+dx, iy+dy
            if light:
                x, y = min(511,max(0,x)), min(511,max(0,y))
                assert (x,y) in atlas, ('unowned bilinear footprint',x,y)
                pixel = atlas[x,y]
            else:
                at = 24+((y % bh)*bw+(x % bw))*4
                pixel = [base[at+c]/255 for c in range(3)]
            for c in range(3):
                rgb[c] += weight*pixel[c]
        return rgb

    depth = (folder/'far-cap.depth').read_bytes()
    assert depth[:12] == b'RFD1'+struct.pack('<II',640,480)
    with Image.open(folder/'far-cap.png') as source:
        assert source.size == (640,480)
        actual = source.convert('RGB').tobytes()
    checked, triangles, largest_error = {}, 0, 0
    for first in range(0,count,3):
        tri = vertices[first:first+3]
        if not all(v[9] == material and v[13] == image for v in tri):
            continue
        triangles += 1
        assert all(v[3:6] == (1.,1.,1.) and v[8] == v[12] and v[8] > 0 for v in tri)
        a,b,c = tri
        determinant = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        assert determinant != 0
        for y in range(max(0,math.floor(min(v[1] for v in tri))), min(480,math.ceil(max(v[1] for v in tri)))):
            for x in range(max(0,math.floor(min(v[0] for v in tri))), min(640,math.ceil(max(v[0] for v in tri)))):
                px,py = x+.5,y+.5
                w0 = ((b[1]-c[1])*(px-c[0])+(c[0]-b[0])*(py-c[1]))/determinant
                w1 = ((c[1]-a[1])*(px-c[0])+(a[0]-c[0])*(py-c[1]))/determinant
                weights = (w0,w1,1-w0-w1)
                # Interior only: shared-edge coverage/rounding is a separate check.
                if min(weights) < .05:
                    continue
                mix = lambda field: sum(weights[i]*tri[i][field] for i in range(3))
                z = mix(2)
                observed_depth = struct.unpack_from('<f',depth,12+(y*640+x)*4)[0]
                if abs(z-observed_depth) > 2:
                    continue
                q = mix(8)
                texture = sample(mix(6)/q,mix(7)/q)
                lighting = sample(mix(10)/q,mix(11)/q,True)
                predicted = [math.floor(min(1,texture[k]*lighting[k]*2)*255+.5) for k in range(3)]
                observed = list(actual[(y*640+x)*3:(y*640+x+1)*3])
                error = max(abs(predicted[k]-observed[k]) for k in range(3))
                assert error <= 1, ('cap pixel mismatch',x,y,predicted,observed,error)
                largest_error = max(largest_error,error)
                checked[x,y] = observed
    assert len(checked) >= 100, ('insufficient visible cap coverage',len(checked))
    return dict(result='PASS',triangles=triangles,visible_interior_pixels=len(checked),
                maximum_channel_error=largest_error,
                observed_channel_range=[min(min(p) for p in checked.values()),max(max(p) for p in checked.values())],
                scope='All depth-matching interior cap pixels in this view; one-code channel tolerance for float rounding. Bilinear footprints remain inside owned atlas tiles. Edges, occluded samples, Xbox command execution and retail visual parity are not covered.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    args = parser.parse_args()
    report = audit(args.folder)
    (args.folder/'pixel-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
