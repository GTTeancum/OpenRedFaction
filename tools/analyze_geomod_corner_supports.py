"""Measure disagreement between recorded supporting-plane combinations.

Input is a diagnostic log with SUPPORT face-id edge-id statusN followed by
twelve plane coefficients. This analyzes plane arithmetic, not mesh closure.
"""
import argparse
import itertools
import json
import math
import struct
from pathlib import Path


def cross(a, b):
    return [a[(i+1) % 3]*b[(i+2) % 3]-a[(i+2) % 3]*b[(i+1) % 3]
            for i in range(3)]


def solve(planes):
    products = [cross(planes[(i+1) % 3], planes[(i+2) % 3]) for i in range(3)]
    determinant = sum(planes[0][i]*products[0][i] for i in range(3))
    if abs(determinant) < 1e-10:
        return None
    return [-sum(planes[i][3]*products[i][j] for i in range(3))/determinant
            for j in range(3)]


def analyze(path):
    combinations = {}
    for line in path.read_text().splitlines():
        if not line.startswith('SUPPORT '):
            continue
        fields = line.split()
        if len(fields) != 16:
            raise ValueError('Expected two IDs, status and twelve coefficients')
        values = struct.unpack('<12f', struct.pack('<12f', *map(float, fields[4:])))
        if not all(map(math.isfinite, values)):
            raise ValueError('Non-finite plane')
        planes = tuple(values[i:i+4] for i in range(0, 12, 4))
        key = tuple(sorted(planes))
        if key not in combinations:
            combinations[key] = dict(planes=planes, position=solve(planes), events=0)
        combinations[key]['events'] += 1
    rows = list(combinations.values())
    points = [r['position'] for r in rows if r['position'] is not None]
    if not points:
        raise ValueError('No nonsingular support combinations')
    all_planes = set(p for r in rows for p in r['planes'])
    for row in rows:
        point = row['position']
        row['maximum_other_plane_residual'] = None if point is None else max(
            abs(sum(p[i]*point[i] for i in range(3))+p[3]) for p in all_planes)
    return dict(scope='Recorded plane combinations, not a weld recommendation or closure proof',
                unique_planes=len(all_planes), combinations=rows,
                maximum_corner_separation=max((math.dist(a, b) for a, b in
                                                itertools.combinations(points, 2)), default=0))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = analyze(args.log)
    output = args.output or args.log.with_suffix('.supports.json')
    output.write_text(json.dumps(report, indent=2)+'\n')
    print('Unique planes:', report['unique_planes'])
    print('Maximum corner separation:', report['maximum_corner_separation'])
    print(output)
