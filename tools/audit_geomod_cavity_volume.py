"""Sample cavity membership against source plus recorded star-cutter tetrahedra.

Offline double-precision audit, not a proof of global nonintersection or game
collision fidelity. Requires NumPy and a convex source RGM1 exported by the
live-history probe; supports its unconstrained star-cutter RFDS fixture only.
"""
import argparse
import json
from pathlib import Path
import struct
import numpy as np


def mesh(data):
    if data[:4] != b'RGM1':
        raise ValueError('Expected RGM1')
    nv, nf = struct.unpack_from('<II', data, 4)
    if not 0 < nv <= 65536 or not 0 < nf <= 65536 or len(data) != 12+nv*20+nf*16:
        raise ValueError('Invalid RGM1 size')
    vertices = np.frombuffer(data, '<f4', nv*5, 12).reshape(nv, 5)[:, :3].astype(float)
    faces = np.frombuffer(data, '<u4', nf*4, 12+nv*20).reshape(nf, 4)
    if not np.isfinite(vertices).all():
        raise ValueError('Nonfinite positions')
    triangles = []
    for first, count, _, _ in faces:
        if not 3 <= count <= 64 or first+count > nv:
            raise ValueError('Invalid face window')
        pieces = np.array([vertices[[first, first+i, first+i+1]] for i in range(1, count-1)])
        # Collinear retained boundary points produce zero-area fan triangles.
        # They contribute zero winding; an entirely degenerate face is invalid.
        nonzero = np.any(np.cross(pieces[:, 1]-pieces[:, 0], pieces[:, 2]-pieces[:, 0]) != 0, axis=1)
        if not np.any(nonzero):
            raise ValueError('Degenerate output face')
        triangles.extend(pieces[nonzero])
    return vertices, np.array(triangles)


def cutters(data):
    if data[:4] != b'RFDS' or len(data) < 316 or data[288:292] != b'RGCH':
        raise ValueError('Expected fixture RFDS with RGCH')
    if struct.unpack_from('<I', data, 4)[0] != 1 or struct.unpack_from('<I', data, 292)[0] != 1:
        raise ValueError('Unsupported checkpoint version')
    if struct.unpack_from('<I', data, 8)[0] != len(data) or struct.unpack_from('<I', data, 304)[0] != 1:
        raise ValueError('Expected cavity checkpoint')
    count = struct.unpack_from('<I', data, 300)[0]
    if not 0 < count <= 16:
        raise ValueError('Unsupported cut count')
    end = 288+struct.unpack_from('<I', data, 252)[0]
    if not 316 <= end <= len(data):
        raise ValueError('Invalid history window')
    offset, result = 316, []
    for _ in range(count):
        star, nv, nf, *kernel = struct.unpack_from('<III3f', data, offset)
        offset += 24
        if star != 1 or not 4 <= nf <= 20 or nv != nf*3 or offset+nv*20+nf*16 > end:
            raise ValueError('Unsupported star record')
        vertices = np.frombuffer(data, '<f4', nv*5, offset).reshape(nv, 5)[:, :3].astype(float)
        offset += nv*20
        faces = np.frombuffer(data, '<u4', nf*4, offset).reshape(nf, 4)
        offset += nf*16
        for first, n, _, _ in faces:
            if n != 3 or first+3 > nv:
                raise ValueError('Unsupported cutter face')
            result.append(np.array([kernel, *vertices[first:first+3]], dtype=float))
    if offset != end or not np.isfinite(result).all():
        raise ValueError('Invalid cutter payload')
    return np.array(result)


def winding(points, triangles):
    values = []
    for start in range(0, len(points), 32):
        vectors = triangles[None, :, :, :] - points[start:start+32, None, None, :]
        a, b, c = vectors[:, :, 0], vectors[:, :, 1], vectors[:, :, 2]
        la, lb, lc = (np.linalg.norm(v, axis=-1) for v in (a, b, c))
        dot = lambda x, y: np.sum(x*y, axis=-1)
        numerator = dot(a, np.cross(b, c))
        denominator = la*lb*lc + dot(a, b)*lc + dot(b, c)*la + dot(c, a)*lb
        values.extend(np.sum(2*np.arctan2(numerator, denominator), axis=1)/(4*np.pi))
    return np.array(values)


def expected_membership(points, source, tetrahedra):
    # Source fixture is convex; independently orient each plane away from its center.
    center = np.mean(source.reshape(-1, 3), axis=0)
    normals = np.cross(source[:, 1]-source[:, 0], source[:, 2]-source[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    if np.any(lengths == 0):
        raise ValueError('Degenerate source triangle')
    normals /= lengths[:, None]
    normals[np.sum((center-source[:, 0])*normals, axis=1) > 0] *= -1
    source_distances = np.einsum('pti,ti->pt', source.reshape(-1, 3)[:, None, :]-source[None, :, 0], normals)
    if np.any(source_distances > 1e-8):
        raise ValueError('Source must be convex')
    distances = np.einsum('pti,ti->pt', points[:, None, :]-source[None, :, 0], normals)
    inside = np.all(distances <= 0, axis=1)
    for tetra in tetrahedra:
        matrix = (tetra[1:]-tetra[0]).T
        weights = (points-tetra[0]) @ np.linalg.inv(matrix).T
        inside |= np.all(weights >= 0, axis=1) & (np.sum(weights, axis=1) <= 1)
    return inside


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--mesh', type=Path, required=True)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    vertices, triangles = mesh(args.mesh.read_bytes())
    _, source = mesh(args.source.read_bytes())
    tetrahedra = cutters(args.checkpoint.read_bytes())
    rng = np.random.default_rng(2001)
    cut_vertices = tetrahedra.reshape(-1, 3)
    points = rng.uniform(cut_vertices.min(axis=0)-.25, cut_vertices.max(axis=0)+.25, (2048, 3))
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    if np.any(lengths == 0):
        raise ValueError('Degenerate output triangle')
    normals /= lengths[:, None]
    centers = triangles.mean(axis=1)
    points = np.concatenate([points, centers+.02*normals, centers-.02*normals])
    expected = expected_membership(points, source, tetrahedra)
    actual = winding(points, triangles)
    rounded = np.rint(actual)
    invalid = (~np.isfinite(actual)) | (np.abs(actual-rounded) > 1e-4) | (np.abs(rounded) > 1)
    source_center = source.reshape(-1, 3).mean(axis=0)
    orientation = int(np.rint(winding(source_center[None, :], source)[0]))
    if abs(orientation) != 1:
        raise ValueError('Source does not have consistent closed orientation')
    wrong = np.flatnonzero(invalid | (rounded != np.where(expected, orientation, 0)))
    def volume(tris):
        a, b, c = (tris[:, i]-source_center for i in range(3))
        return float(np.sum(a*np.cross(b, c))/6)
    source_volume, output_volume = volume(source), volume(triangles)
    report = dict(result='PASS' if not len(wrong) else 'FAIL', samples=len(points),
                  mismatches=len(wrong), tetrahedra=len(tetrahedra),
                  source_orientation=orientation, source_volume=source_volume, output_volume=output_volume,
                  added_cavity_volume=orientation*(output_volume-source_volume),
                  examples=[dict(point=points[i].tolist(), expected=bool(expected[i]), winding=float(actual[i])) for i in wrong[:20]],
                  winding_min=float(actual.min()), winding_max=float(actual.max()),
                  scope='Deterministic sampled union membership, including both sides of output triangles; not a global nonintersection or collision proof.')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if not len(wrong) else 2


if __name__ == '__main__':
    raise SystemExit(main())
