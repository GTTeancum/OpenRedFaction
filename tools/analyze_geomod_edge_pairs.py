"""Find opposed physical edges without prefiltering away angular mismatches."""
import argparse
import csv
import json
import math
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--edge', type=int, nargs=2, required=True, metavar=('FACE', 'CORNER'))
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    faces = {}
    for row in csv.DictReader(args.csv.open()):
        p = [struct.unpack('<f', struct.pack('<f', float(row[k])))[0] for k in ('x','y','z')]
        faces.setdefault(int(row['face']), {})[int(row['corner'])] = p
    dot = lambda a,b: sum(x*y for x,y in zip(a,b))
    sub = lambda a,b: [x-y for x,y in zip(a,b)]
    face, corner = args.edge
    a = faces[face][corner]
    b = faces[face][(corner+1)%len(faces[face])]
    direction = sub(b,a)
    length2 = dot(direction,direction)
    if length2 == 0:
        raise ValueError('Degenerate query edge')
    mid = [(x+y)*.5 for x,y in zip(a,b)]
    pairs = []
    for f, points in faces.items():
        for e,x in points.items():
            y = points[(e+1)%len(points)]
            edge = sub(y,x)
            size = dot(edge,edge)
            if size == 0:
                raise ValueError('Degenerate candidate edge')
            direction_dot = dot(direction,edge)
            along = dot(sub(mid,x),edge)/size
            if direction_dot >= 0 or not 1e-8 < along < 1-1e-8:
                continue
            residual = [v-z-along*d for v,z,d in zip(mid,x,edge)]
            distance = math.sqrt(dot(residual,residual))
            sine2 = abs(direction_dot*direction_dot-length2*size)/(length2*size)
            pairs.append(dict(face=f, edge=e, start=x, end=y, distance=distance,
                angular_residual=sine2, passes_distance=distance < 1e-6,
                passes_angle=sine2 < 1e-8))
    pairs.sort(key=lambda p:p['distance'])
    result = dict(edge=args.edge, start=a, end=b, length=math.sqrt(length2),
        closest_opposed=pairs[:5], scope='Diagnostic only: preserves current closure thresholds; proximity does not prove shared topology.')
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
