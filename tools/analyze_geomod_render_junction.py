"""Measure a render T-junction from the opt-in final terrain CSV export."""
import argparse
import csv
import json
import math
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--edge', type=int, nargs=3, required=True, metavar=('FACE', 'A', 'B'))
    parser.add_argument('--point', type=int, nargs=2, required=True, metavar=('FACE', 'CORNER'))
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    rows = list(csv.DictReader(args.csv.open()))
    def position(face, corner):
        row = next(r for r in rows if int(r['face']) == face and int(r['corner']) == corner)
        return [struct.unpack('<f', struct.pack('<f', float(row[k])))[0] for k in ('x', 'y', 'z')]
    a, b = [position(args.edge[0], c) for c in args.edge[1:]]
    p = position(*args.point)
    delta = [y-x for x, y in zip(a, b)]
    length = sum(d*d for d in delta)
    if not length:
        raise ValueError('Degenerate edge')
    fraction = sum((v-x)*d for v, x, d in zip(p, a, delta))/length
    residual = [v-(x+fraction*d) for v, x, d in zip(p, a, delta)]
    distance = math.sqrt(sum(v*v for v in residual))
    def half_step(value):
        exponent = (struct.unpack('<I', struct.pack('<f', value))[0] >> 23) & 255
        return math.ldexp(1.0, exponent-151 if exponent else -150)
    rounding = [half_step(v)+(1-fraction)*half_step(x)+fraction*half_step(y)
                for v, x, y in zip(p, a, b)]
    result = dict(edge=args.edge, point=args.point, a=a, b=b, position=p,
                  fraction=fraction, residual=residual, distance=distance,
                  legacy_distance_gate=1e-6, inside_segment=0 < fraction < 1,
                  passes_distance_gate=distance <= 1e-6,
                  rounding_bounds=rounding,
                  passes_rounding_gate=all(abs(v) <= limit for v, limit in zip(residual, rounding)),
                  scope='Float32-restored render geometry; geometric proximity alone does not establish shared topology.')
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
