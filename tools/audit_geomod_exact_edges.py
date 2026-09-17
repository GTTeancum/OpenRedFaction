"""Audit exact directed-edge pairing in RGM1; no welding or proximity matching.

This diagnoses combinatorial closure only. It does not prove absence of
self-intersections, correct cavity volume, collision behavior or visual fidelity.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import struct


def audit(data):
    if len(data) < 12 or data[:4] != b'RGM1':
        raise ValueError('Expected RGM1 mesh')
    nv, nf = struct.unpack_from('<II', data, 4)
    if not 0 < nv <= 65536 or not 0 < nf <= 65536 or len(data) != 12 + nv*20 + nf*16:
        raise ValueError('Invalid mesh counts or byte length')
    vertices = [struct.unpack_from('<5f', data, 12+i*20) for i in range(nv)]
    if not all(math.isfinite(x) for v in vertices for x in v):
        raise ValueError('Nonfinite vertex')
    edges = Counter()
    owners = {}
    links = {}
    for face in range(nf):
        first, count, _, _ = struct.unpack_from('<4I', data, 12+nv*20+face*16)
        if not 3 <= count <= 64 or first+count > nv:
            raise ValueError('Invalid face window')
        for i in range(count):
            edge = vertices[first+i][:3], vertices[first+(i+1)%count][:3]
            edges[edge] += 1
            owners.setdefault(edge, []).append([face, i])
            previous = vertices[first+(i-1)%count][:3]
            link = links.setdefault(edge[0], {})
            link.setdefault(previous, Counter())[edge[1]] += 1
            link.setdefault(edge[1], Counter())[previous] += 1
    bad = []
    for (a, b), count in edges.items():
        reverse = edges[b, a]
        if a == b or count != 1 or reverse != 1:
            bad.append(dict(start=a, end=b, forward=count, reverse=reverse, owners=owners[a, b]))
    bad_vertices = []
    for vertex, link in links.items():
        seen, pending = set(), [next(iter(link))]
        while pending:
            node = pending.pop()
            if node in seen:
                continue
            seen.add(node)
            pending.extend(link[node])
        # A closed manifold vertex has one cyclic link, not two shells touching
        # at a point. Preserve multiplicity when checking the link's degrees.
        if vertex in link or len(seen) != len(link) or any(sum(v.values()) != 2 for v in link.values()):
            bad_vertices.append(dict(position=vertex, neighbors=len(link), connected=len(seen)))
    return dict(exact_pairs=not bad, vertex_manifold=not bad_vertices,
                combinatorial_closed=not bad and not bad_vertices,
                nonmanifold_vertices=bad_vertices,
                vertices=nv, faces=nf, directed_edges=sum(edges.values()),
                unmatched_count=len(bad), unmatched=bad,
                scope='Exact edge pairs and cyclic vertex links; T-junctions and float discrepancies remain unmatched. No self-intersection, volume or fidelity claim.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mesh', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = audit(args.mesh.read_bytes())
    text = json.dumps(result, indent=2)+'\n'
    if args.output:
        args.output.write_text(text)
    print(text)
    return 0 if result['combinatorial_closed'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
