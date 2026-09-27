"""Rank compiled room faces near a scripted blast by authored brush owner.

This is read-only geometry evidence. Distance to a face is not the original
GeoMod admission result or proof that the room's Boolean solid is editable.
"""
import argparse
import json
import math
import struct

from inspect_geomod_source_topology import load
from probe_campaign_geomod_brush import U32, parse_record, section_bytes


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def segment_distance(point, a, b):
    edge = sub(b, a)
    size = dot(edge, edge)
    t = max(0.0, min(1.0, dot(sub(point, a), edge) / size)) if size else 0.0
    near = tuple(a[i] + t * edge[i] for i in range(3))
    return math.sqrt(dot(sub(point, near), sub(point, near)))


def triangle_distance(point, a, b, c):
    u, v = sub(b, a), sub(c, a)
    normal = (u[1] * v[2] - u[2] * v[1],
              u[2] * v[0] - u[0] * v[2],
              u[0] * v[1] - u[1] * v[0])
    length2 = dot(normal, normal)
    if length2:
        plane = dot(sub(point, a), normal)
        projected = tuple(point[i] - normal[i] * plane / length2 for i in range(3))
        w = sub(projected, a)
        uu, uv, vv = dot(u, u), dot(u, v), dot(v, v)
        uw, vw = dot(u, w), dot(v, w)
        den = uu * vv - uv * uv
        if den:
            s, t = (vv * uw - uv * vw) / den, (uu * vw - uv * uw) / den
            if s >= -1e-7 and t >= -1e-7 and s + t <= 1 + 1e-7:
                return abs(plane) / math.sqrt(length2)
    return min(segment_distance(point, a, b), segment_distance(point, b, c),
               segment_distance(point, c, a))


def face_distance(point, vertices):
    if len(vertices) < 3:
        raise ValueError("compiled face has fewer than three corners")
    return min(triangle_distance(point, vertices[0], vertices[i], vertices[i + 1])
               for i in range(1, len(vertices) - 1))


def owners(level):
    data, _ = section_bytes(level)
    count = U32.unpack_from(data)[0]
    offset = 4
    by_source = {}
    for index in range(count):
        if offset + 4 > len(data):
            raise ValueError("short editor brush UID")
        uid = U32.unpack_from(data, offset)[0]
        brush = parse_record(data, offset, uid)
        for face in brush["faces"]:
            word = face["source_word"]
            if word in by_source:
                raise ValueError(f"duplicate source word {word}")
            by_source[word] = (uid, brush["tail"][2], index)
        offset += brush["bytes"]
    if offset != len(data):
        raise ValueError(f"{len(data) - offset} unparsed editor bytes")
    return by_source, count


def locate(level, room, point, limit):
    by_source, brush_count = owners(level)
    compiled, meta = load(level)
    rows = []
    for face in compiled:
        if face["room"] != room:
            continue
        owner = by_source.get(face["source_word"])
        rows.append({"face": face["id"], "distance": face_distance(point, face["points"]),
                     "source_word": face["source_word"], "owner_uid": owner[0] if owner else None,
                     "owner_operation": owner[1] if owner else None,
                     "owner_index": owner[2] if owner else None,
                     "flags": face["flags"], "portal": face["portal"]})
    rows.sort(key=lambda row: (row["distance"], row["face"]))
    return {"level": level, "room": room, "point": point,
            "editor_brushes": brush_count, "compiled_faces_in_room": len(rows),
            "geometry_sha256": meta["geometry_sha256"], "nearest": rows[:limit],
            "scope": "Triangulated compiled polygon distance and exact source-word ownership; not a solid or cutter query"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--level", default="L1S1.rfl")
    parser.add_argument("--room", type=int, default=28)
    parser.add_argument("--point", type=float, nargs=3, default=(36.7616, 4.0061, 44.4250))
    parser.add_argument("--limit", type=int, default=12)
    args = parser.parse_args()
    if args.limit < 1 or args.limit > 100:
        parser.error("limit must be 1..100")
    print(json.dumps(locate(args.level, args.room, args.point, args.limit), indent=2))
