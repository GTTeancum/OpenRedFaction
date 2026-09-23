"""Inspect one installed v180 editor brush without assuming every earlier record parses.

This is a read-only source/compiled-ownership probe, not a runtime CSG loader.
"""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import struct

from inspect_geomod_source_topology import convex, load, topology

ROOT = Path(__file__).resolve().parents[1]
U32 = struct.Struct("<I")
VEC3 = struct.Struct("<3f")


def section_bytes(level):
    levels = json.loads((ROOT / "artifacts/levels.json").read_text())
    inventory = json.loads((ROOT / "artifacts/inventory.json").read_text())
    row = next(x for x in levels if x["file"].lower() == level.lower())
    archive = next(x for x in inventory["files"] if x["path"] == row["archive"])
    entry = next(x for x in archive["vpp"]["entries"] if x["name"].lower() == level.lower())
    section = next(x for x in row["sections"] if x["type"] == "0x2000000")
    with (ROOT / "Installed_Game" / row["archive"]).open("rb") as stream:
        stream.seek(entry["offset"] + section["offset"] + 8)
        data = stream.read(section["size"])
    if len(data) != section["size"]:
        raise ValueError("short editor-brush section")
    return data, row["archive"]


def parse_at(data, at, wanted):
    start = at

    def take(size):
        nonlocal at
        if size < 0 or at + size > len(data):
            raise ValueError("brush runs past section")
        result = data[at : at + size]
        at += size
        return result

    def number():
        return U32.unpack(take(4))[0]

    if number() != wanted:
        raise ValueError("wrong brush UID")
    position = VEC3.unpack(take(12))
    raw_basis = struct.unpack("<9f", take(36))
    basis = raw_basis[3:] + raw_basis[:3]  # RED4d0aa0 file-to-runtime rotation
    if any(not math.isfinite(x) for x in (*position, *basis)):
        raise ValueError("nonfinite brush transform")
    norms = [sum(basis[j * 3 + i] ** 2 for j in range(3)) for i in range(3)]
    if max(abs(n - 1) for n in norms) > 0.01:
        raise ValueError("implausible brush basis")
    take(6)
    textures = []
    for _ in range(number()):
        if len(textures) == 64:
            raise ValueError("too many brush textures")
        length = struct.unpack("<H", take(2))[0]
        if not length or length > 255:
            raise ValueError("invalid texture name")
        textures.append(take(length).decode("cp1252"))
    take(16)
    count = number()
    if not 4 <= count <= 8192:
        raise ValueError("invalid vertex count")
    vertices = [VEC3.unpack(take(12)) for _ in range(count)]
    if any(not math.isfinite(x) for v in vertices for x in v):
        raise ValueError("nonfinite brush vertex")
    face_count = number()
    if not 4 <= face_count <= 256:
        raise ValueError("invalid face count")

    def world(vertex):
        return tuple(sum(vertex[j] * basis[j * 3 + i] for j in range(3)) + position[i] for i in range(3))

    faces = []
    for index in range(face_count):
        raw = take(56)
        material = U32.unpack_from(raw, 16)[0]
        mapping = U32.unpack_from(raw, 20)[0]
        source_word = U32.unpack_from(raw, 24)[0]
        corners = U32.unpack_from(raw, 52)[0]
        if material >= len(textures) or not 3 <= corners <= 64:
            raise ValueError("invalid face header")
        points = []
        for _ in range(corners):
            corner = take(12 if mapping == 0xFFFFFFFF else 20)
            vertex = U32.unpack_from(corner)[0]
            if vertex >= len(vertices):
                raise ValueError("corner vertex outside brush")
            points.append(world(vertices[vertex]))
        faces.append({"id": index, "source_word": source_word, "texture": material, "points": points})
    tail = struct.unpack("<5I", take(20))
    return {"uid": wanted, "offset": start, "bytes": at - start, "position": position,
            "textures": textures, "vertices": len(vertices), "faces": faces, "tail": tail}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("level")
    parser.add_argument("uid", type=int)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    data, archive = section_bytes(args.level)
    matches = []
    needle = U32.pack(args.uid)
    offset = -1
    while True:
        offset = data.find(needle, offset + 1)
        if offset < 0:
            break
        try:
            matches.append(parse_at(data, offset, args.uid))
        except (ValueError, struct.error, OverflowError):
            pass
    if len(matches) != 1:
        raise ValueError(f"expected one valid brush UID {args.uid}, found {len(matches)}")
    brush = matches[0]
    compiled, _ = load(args.level)
    sources = {f["source_word"] for f in brush["faces"]}
    linked = [f for f in compiled if f["source_word"] in sources]
    report = {"level": args.level, "archive": archive, "section_sha256": hashlib.sha256(data).hexdigest(),
              "brush": brush, "topology": topology(brush["faces"]),
              "solid": convex(brush["faces"], False), "cavity": convex(brush["faces"], True),
              "compiled_faces": len(linked), "compiled_rooms": dict(collections.Counter(f["room"] for f in linked)),
              "compiled_source_counts": dict(collections.Counter(f["source_word"] for f in linked)),
              "scope": "Read-only editor brush and compiled face ownership; no runtime cut or solid decomposition"}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"level": args.level, "uid": args.uid, "offset": brush["offset"],
                      "source_faces": len(brush["faces"]), "closed": report["topology"]["closed_oriented"],
                      "solid_convex": report["solid"]["convex"], "cavity_convex": report["cavity"]["convex"],
                      "compiled_faces": len(linked), "rooms": report["compiled_rooms"]}))


if __name__ == "__main__":
    main()
