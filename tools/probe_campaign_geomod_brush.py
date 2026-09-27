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
    # Campaign records include two-face detail geometry as well as large
    # room volumes (L1S1 UID7778 has 488 faces).
    if not 1 <= face_count <= 8192:
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


def parse_record(data, at, wanted, recovery_window=65536):
    """Keep unidentified editor sidecars opaque and require a valid next header.

    The common record ends at the five-word tail. Some campaign detail brushes
    carry extra data before that tail; the first small-flags tail directly
    followed by another validated brush header is a recovery candidate. The
    inventory must still consume its declared count and exact section size.
    """
    brush = parse_at(data, at, wanted)
    brush["opaque_bytes"] = 0
    if brush["tail"][2] <= 255:
        return brush
    nominal_end = at + brush["bytes"]
    match = None
    for following in range(nominal_end + 1,
                           min(len(data) - 4, nominal_end + recovery_window) + 1):
        tail = struct.unpack_from("<5I", data, following - 20)
        if tail[2] > 255:
            continue
        try:
            successor = parse_at(data, following, U32.unpack_from(data, following)[0])
        except (ValueError, struct.error, OverflowError):
            continue
        if successor["uid"] == wanted:
            continue
        match = (following, tail)
        break
    if match is None:
        raise ValueError(f"missing opaque tail after brush {wanted}")
    following, tail = match
    brush["opaque_bytes"] = following - nominal_end
    brush["bytes"] = following - at
    brush["tail"] = tail
    return brush


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("level")
    parser.add_argument("uid", type=int, nargs="?")
    parser.add_argument("--inventory", action="store_true",
                        help="List the validated uniform prefix and first nonuniform offset")
    parser.add_argument("--resync-window", type=int, default=65536,
                        help="After prefix stops, probe this many bytes for candidate record headers (L1S2 needs 17568)")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if not 0 <= args.resync_window <= 65536:
        parser.error("resync window must be 0..65536 bytes")
    data, archive = section_bytes(args.level)
    if args.inventory:
        if args.uid is not None:
            parser.error("uid and --inventory are mutually exclusive")
        declared = U32.unpack_from(data)[0]
        offset = 4
        rows = []
        compiled, _ = load(args.level)
        by_source = collections.defaultdict(list)
        for face in compiled:
            by_source[face["source_word"]].append(face["room"])
        stopped = None
        for index in range(declared):
            if offset + 4 > len(data):
                stopped = {"index": index, "offset": offset, "reason": "short UID"}
                break
            uid = U32.unpack_from(data, offset)[0]
            try:
                brush = parse_record(data, offset, uid, args.resync_window)
            except (ValueError, struct.error, OverflowError) as exc:
                stopped = {"index": index, "offset": offset, "reason": str(exc)}
                break
            rooms = collections.Counter(room for face in brush["faces"]
                                        for room in by_source[face["source_word"]])
            rows.append({"index": index, "uid": uid, "offset": offset,
                         "bytes": brush["bytes"], "opaque_bytes": brush["opaque_bytes"],
                         "operation": brush["tail"][2],
                         "source_faces": len(brush["faces"]),
                         "compiled_faces": sum(rooms.values()), "rooms": dict(rooms)})
            offset += brush["bytes"]
        candidates = []
        if stopped and 0 <= args.resync_window <= 65536:
            for seek in range(offset + 1, min(len(data) - 4, offset + args.resync_window) + 1):
                uid = U32.unpack_from(data, seek)[0]
                try:
                    brush = parse_at(data, seek, uid)
                except (ValueError, struct.error, OverflowError):
                    continue
                following = seek + brush["bytes"]
                next_valid = False
                if following + 4 <= len(data):
                    try:
                        parse_at(data, following, U32.unpack_from(data, following)[0])
                        next_valid = True
                    except (ValueError, struct.error, OverflowError):
                        pass
                candidates.append({"offset": seek, "gap": seek - offset,
                                   "uid": uid, "bytes": brush["bytes"],
                                   "next_valid": next_valid})
                if len(candidates) == 32:
                    break
        result = {"level": args.level, "archive": archive,
                  "section_sha256": hashlib.sha256(data).hexdigest(),
                  "declared": declared, "parsed": len(rows), "consumed_bytes": offset,
                  "complete": stopped is None and len(rows) == declared and offset == len(data),
                  "stopped": stopped,
                  "resync_candidates": candidates, "rows": rows,
                  "scope": "Read-only editor records; opaque spans are bounded by validated successor headers, not decoded as CSG"}
        if stopped is None and offset != len(data):
            raise ValueError(f"{len(data) - offset} trailing section bytes")
        if args.report:
            args.report.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({key: value for key, value in result.items() if key != "rows"}))
        return
    if args.uid is None:
        parser.error("uid is required unless --inventory is set")
    matches = []
    needle = U32.pack(args.uid)
    offset = -1
    while True:
        offset = data.find(needle, offset + 1)
        if offset < 0:
            break
        try:
            matches.append(parse_record(data, offset, args.uid, args.resync_window))
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
              "operation": brush["tail"][2],
              "compiled_faces": len(linked), "compiled_rooms": dict(collections.Counter(f["room"] for f in linked)),
              "compiled_source_counts": dict(collections.Counter(f["source_word"] for f in linked)),
              "scope": "Read-only editor brush and compiled face ownership; no runtime cut or solid decomposition"}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"level": args.level, "uid": args.uid, "offset": brush["offset"],
                      "operation": report["operation"],
                      "source_faces": len(brush["faces"]), "closed": report["topology"]["closed_oriented"],
                      "solid_convex": report["solid"]["convex"], "cavity_convex": report["cavity"]["convex"],
                      "compiled_faces": len(linked), "rooms": report["compiled_rooms"]}))


if __name__ == "__main__":
    main()
