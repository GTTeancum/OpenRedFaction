"""Audit compiled air-brush ownership; never admit geometry to the live scene."""
import collections
import hashlib
import json
import math
import struct
from pathlib import Path
from inspect_geomod_source_topology import load, topology, convex

ROOT = Path(__file__).resolve().parents[1]
SUPPORTED = {93, 94, 95, 96, 97, 98}


def normal(points):
    n = [sum(a[(k + 1) % 3] * b[(k + 2) % 3] - a[(k + 2) % 3] * b[(k + 1) % 3]
             for a, b in zip(points, points[1:] + points[:1])) for k in range(3)]
    length = math.sqrt(sum(x * x for x in n))
    assert length > 1e-10, 'Degenerate source/compiled polygon'
    return [x / length for x in n]


def main():
    source = ROOT / 'artifacts/future-vehicles-re/ctf06-editor-brushes.json'
    brushes = json.loads(source.read_text(encoding='utf-8'))
    faces, metadata = load('ctf06.rfl')
    owners = {}
    f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
    for brush in brushes:
        basis = brush['basis'][3:] + brush['basis'][:3]
        brush['world'] = []
        for face in brush['faces']:
            points = [tuple(f32(f32(sum(p[j] * basis[j * 3 + k] for j in range(3))) + brush['position'][k])
                            for k in range(3)) for p in face['points']]
            world = dict(face, points=points)
            brush['world'].append(world)
            token = face['source_word']
            assert token not in owners, 'Ambiguous authored face identity'
            owners[token] = (brush, world)
    grouped = collections.defaultdict(list)
    unowned = []
    for face in faces:
        if face['room'] != 3:
            continue
        owner = owners.get(face['source_word'])
        if owner is None:
            unowned.append(dict(id=face['id'], flags=face['flags'], source=face['source_word']))
        else:
            grouped[owner[0]['uid']].append(face)
    rows = []
    for brush in brushes:
        compiled = grouped.get(brush['uid'], [])
        if not compiled:
            continue
        residual = 0.0
        alignment = 1.0
        for face in compiled:
            authored = owners[face['source_word']][1]
            n = normal(authored['points'])
            offset = -sum(n[k] * authored['points'][0][k] for k in range(3))
            residual = max(residual, max(abs(offset + sum(n[k] * p[k] for k in range(3))) for p in face['points']))
            other = normal(face['points'])
            alignment = min(alignment, sum(x * y for x, y in zip(n, other)))
        flags = brush['tail'][2]
        rows.append(dict(uid=brush['uid'], index=brush['index'], brush_flags=flags,
                         supported=brush['uid'] in SUPPORTED, authored_faces=len(brush['world']),
                         compiled_faces=len(compiled), compiled_ids=[f['id'] for f in compiled],
                         authored_topology=topology(brush['world']), compiled_topology=topology(compiled),
                         inward_convex=convex(brush['world'], True), outward_convex=convex(brush['world'], False),
                         plane_residual=residual, minimum_winding_alignment=alignment))
    counts = collections.Counter()
    for row in rows:
        counts[str(row['brush_flags'])] += row['compiled_faces']
    report = dict(scope='Installed room3 compiled-to-authored ownership, plane and topology audit; no ordered CSG or live admission claim',
                  geometry_sha256=metadata['geometry_sha256'], export_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  room_faces=sum(f['room'] == 3 for f in faces), owned_by_brush_flags=dict(counts),
                  supported_faces=sum(r['compiled_faces'] for r in rows if r['supported']), unowned=unowned, rows=rows)
    assert sum(counts.values()) + len(unowned) == report['room_faces']
    assert all(f['flags'] & 4 for f in unowned), 'Unexplained non-liquid compiled face'
    target = next(r for r in rows if r['uid'] == 66)
    assert target['brush_flags'] == 2 and target['authored_topology']['closed_oriented']
    assert target['inward_convex']['convex'] and target['plane_residual'] < 1e-5
    assert target['minimum_winding_alignment'] > .99999
    # Conservative AABB neighborhood for a first wall cut, not CSG admission.
    center, radius = (-33.0, 4.0, 8.0), 1.1
    neighbors = []
    for brush in brushes:
        points = [p for face in brush['world'] for p in face['points']]
        bounds = [(min(p[k] for p in points), max(p[k] for p in points)) for k in range(3)]
        if all(lo - radius <= x <= hi + radius for x, (lo, hi) in zip(center, bounds)):
            neighbors.append(brush['uid'])
    wall = faces[5964]
    assert wall['room'] == 3 and wall['source_word'] == 398
    n = normal(wall['points'])
    assert abs(sum(n[k] * (center[k] - wall['points'][0][k]) for k in range(3))) < 1e-5
    margins = []
    for a, b in zip(wall['points'], wall['points'][1:] + wall['points'][:1]):
        edge = [b[k] - a[k] for k in range(3)]
        offset = [center[k] - a[k] for k in range(3)]
        cross = [edge[(k+1)%3]*offset[(k+2)%3]-edge[(k+2)%3]*offset[(k+1)%3] for k in range(3)]
        margins.append(sum(n[k]*cross[k] for k in range(3))/math.sqrt(sum(x*x for x in edge)))
    assert neighbors == [66] and min(margins) > radius
    report['first_wall_candidate'] = dict(center=center, radius=radius, compiled_face=5964,
        authored_face=398, aabb_neighbors=neighbors, minimum_compiled_edge_margin=min(margins),
        scope='Isolated conservative brush bounds and polygon interior only; no cut, visibility or collision qualification')
    output = ROOT / 'artifacts/geomod-cavity-candidates.json'
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('room_faces', 'owned_by_brush_flags', 'supported_faces')}))
    print('air66', json.dumps({k: target[k] for k in ('authored_faces', 'compiled_faces', 'plane_residual', 'minimum_winding_alignment')}))
    print('air66 compiled boundary edges', target['compiled_topology']['boundary_edges'], 'report', output)


if __name__ == '__main__':
    main()
