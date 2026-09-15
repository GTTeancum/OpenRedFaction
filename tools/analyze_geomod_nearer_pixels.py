"""Identify the actual raster triangles behind every nearer-depth discrepancy.

Consumes the paired native replay mesh/depth exports. This is read-only evidence;
it does not relax the depth gate or establish unsnapped world-space geometry.
"""
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import struct
import numpy as np


def load_mesh(path):
    raw = path.read_bytes()
    if raw[:4] != b'RFM1':
        raise ValueError('Expected RFM1')
    count, world, stride = struct.unpack_from('<3I', raw, 4)
    if stride != 56 or world > count or world % 3 or len(raw) != 16 + count * stride:
        raise ValueError('Invalid mesh bounds')
    words = np.frombuffer(raw, dtype='<u4', offset=16).reshape(count, 14)[:world]
    floats = words.view('<f4').reshape(-1, 3, 14)
    return floats[:, :, :3], words.reshape(-1, 3, 14)


def winner(mesh, x, y, expected):
    pos, words = mesh
    a, b, c = pos[:, 0], pos[:, 1], pos[:, 2]
    def edge(p, q, x, y):
        return (x-p[:, 0])*(q[:, 1]-p[:, 1])-(y-p[:, 1])*(q[:, 0]-p[:, 0])
    area = edge(a, b, c[:, 0], c[:, 1])
    with np.errstate(divide='ignore', invalid='ignore'):
        u = edge(b, c, np.float32(x+.5), np.float32(y+.5))/area
        v = edge(c, a, np.float32(x+.5), np.float32(y+.5))/area
        w = np.float32(1)-u-v
        z = u*a[:, 2]+v*b[:, 2]+w*c[:, 2]
    valid = (np.abs(area) >= .00001) & (u >= 0) & (v >= 0) & (w >= 0)
    ids = np.flatnonzero(valid)
    if not len(ids):
        raise ValueError(f'No world coverage at {x},{y}')
    i = int(ids[np.argmin(z[ids])])
    # Float operation contraction/compiler rounding can differ by a few units.
    error = float(z[i])-float(expected)
    if abs(error) > 4:
        raise ValueError(f'Raster reconstruction mismatch at {x},{y}: {error}')
    return dict(first_vertex=i*3, material=int(words[i, 0, 9]),
                lightmap=int(words[i, 0, 13]), depth=float(z[i]),
                reconstruction_error=error, screen_vertices=pos[i].tolist())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    depths = {}
    meshes = {}
    for mode in ('cut', 'intact'):
        raw = (args.folder/(mode+'.depth')).read_bytes()
        if raw[:4] != b'RFD1' or struct.unpack_from('<2I', raw, 4) != (640, 480) or len(raw) != 12+640*480*4:
            raise ValueError('Expected 640x480 depth export')
        depths[mode] = np.frombuffer(raw, dtype='<f4', offset=12).reshape(480, 640)
        meshes[mode] = load_mesh(args.folder/(mode+'.mesh'))
    yy, xx = np.where(depths['cut'][:350]-depths['intact'][:350] < -128)
    rows = []
    for y, x in zip(yy, xx):
        row = dict(pixel=[int(x), int(y)], delta=float(depths['cut'][y, x]-depths['intact'][y, x]))
        for mode in ('cut', 'intact'):
            row[mode] = winner(meshes[mode], int(x), int(y), depths[mode][y, x])
        rows.append(row)
    pairs = Counter((r['cut']['material'], r['intact']['material'], r['cut']['lightmap'], r['intact']['lightmap']) for r in rows)
    floor_evidence = None
    source_path = args.folder/'cut.terrain.csv'
    if source_path.exists() and rows:
        source = list(csv.DictReader(source_path.open()))
        floors = [r for r in source if int(r['source']) != 0xffffffff and
                  tuple(float(r[k]) for k in ('nx', 'ny', 'nz')) == (0, 1, 0)]
        heights = {float(r['y']) for r in floors}
        if len(heights) != 1 or any(float(r['y']) != -float(r['d']) for r in floors):
            raise ValueError('Expected one exact authored upward-facing floor plane')
        floor_y = heights.pop()
        audit = json.loads((args.folder/'report.json').read_text())
        camera = struct.unpack('<12f', struct.pack('<12I', *audit['camera_words'][1:]))
        if camera[6:9] != (0, 1, 0):
            raise ValueError('Floor rounding analysis requires level camera pitch')
        offset = floor_y-camera[1]
        if offset >= 0:
            raise ValueError('Camera must be above floor')
        depth_scale = (1000/999.9)*16777215
        rounding_bound = depth_scale*.1/(320*abs(offset))/16
        modes = {}
        for mode in ('cut', 'intact'):
            mesh = meshes[mode]
            firsts = sorted({r[mode]['first_vertex'] for r in rows})
            residuals = []
            for first in firsts:
                # Reconstruct original projected Y from the exact floor and
                # the emitted reciprocal depth; compare with floored screen Y.
                words = mesh[1][first//3]
                q = words.view('<f4')[:, 8].astype(float)
                screen_y = mesh[0][first//3, :, 1].astype(float)
                residuals.extend((240-320*offset*q-screen_y).tolist())
            deltas = []
            for row in rows:
                x, y = row['pixel']
                q = (240-(y+.5))/(320*offset)
                analytic = depth_scale*(1-.1*q)
                deltas.append(float(depths[mode][y, x])-analytic)
            matches = min(residuals) >= -.0001 and max(residuals) <= 1/16+.0001
            bounded = min(deltas) >= -rounding_bound-4 and max(deltas) <= 4
            modes[mode] = dict(triangles=len(firsts), snap_y_residual_range=[min(residuals), max(residuals)],
                               depth_error_vs_exact_floor_range=[min(deltas), max(deltas)],
                               matches_authored_floor_within_screen_rounding=matches,
                               within_rounding_depth_bound=bounded)
        floor_evidence = dict(authored_y=floor_y, authored_corner_records=len(floors),
                              max_floor_rounding_depth_error=rounding_bound, modes=modes,
                              scope='All flagged pixels only; geometric floor evidence and analytical projection bound, not a relaxed pass threshold.')
    report = dict(count=len(rows), floor_evidence=floor_evidence, surface_pairs=[dict(cut_material=k[0], intact_material=k[1], cut_lightmap=k[2], intact_lightmap=k[3], pixels=v) for k, v in pairs.items()], pixels=rows,
                  limitation='Attribution covers flagged pixels only; optional authored-floor evidence does not establish general crater or original-game parity. Strict depth gates unchanged.')
    (args.folder/'nearer-surfaces.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='pixels'}, indent=2))


if __name__ == '__main__':
    main()
