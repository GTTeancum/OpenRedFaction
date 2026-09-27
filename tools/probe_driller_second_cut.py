"""Check the recorded L1S2 frame-296 Driller star against authored UID8219.

Read-only geometry probe. Each star triangle and its interior kernel form one
tetrahedron; a positive half-space intersection with UID8219 proves that the
second cut reaches another authored brush, beyond an AABB false positive.
"""
import json
import struct

import numpy as np
from scipy.optimize import linprog
from scipy.spatial import ConvexHull

from inspect_geomod_source_topology import convex, load
from probe_campaign_geomod_brush import parse_record, section_bytes


def brushes():
    data, _ = section_bytes("L1S2.rfl")
    count = struct.unpack_from("<I", data)[0]
    at = 4
    for _ in range(count):
        current = struct.unpack_from("<I", data, at)[0]
        record = parse_record(data, at, current)
        yield record
        at += record["bytes"]


def brush(uid):
    for record in brushes():
        if record["uid"] == uid:
            return record
    raise ValueError(f"missing brush {uid}")


def cutter():
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    data = (root / "build/data/driller-single.bin").read_bytes()
    if data[:4] != b"RFCT" or struct.unpack_from("<I", data, 4)[0] != 1:
        raise ValueError("unexpected Driller template")
    faces, scale = struct.unpack_from("<If", data, 8)
    if faces != 26:
        raise ValueError("unexpected Driller face count")
    kernel = np.array(struct.unpack_from("<3f", data, 16))
    local = np.array([struct.unpack_from("<3f", data, 28 + i * 20)
                      for i in range(faces * 3)])
    # Process-local frame-296 basis and contact from the L1S2 330-frame replay.
    basis = np.array([[-0.287775129, 1.59786077e-06, -0.957698166],
                      [-0.00494431565, 0.999986291, 0.0014873792],
                      [0.957684517, 0.00516320998, -0.287771463]])
    center = np.array([124.314514, -2.12633848, -17.0])
    points, interior = center + local @ basis, center + kernel @ basis
    # The live region supplies one downward shallow limit, depth 0.4. Only
    # vertices below the aligned center move, by depth/template radius.
    below = points[:, 1] < center[1]
    points[below, 1] = center[1] + (points[below, 1] - center[1]) * 0.4 / scale
    if interior[1] < center[1]:
        interior[1] = center[1] + (interior[1] - center[1]) * 0.4 / scale
    return points, interior, scale


def clip_polygon(poly, plane):
    result = []
    for a, b in zip(poly, poly[1:] + poly[:1]):
        da, db = np.dot(plane[:3], a) + plane[3], np.dot(plane[:3], b) + plane[3]
        if da <= 0:
            result.append(a)
        if (da < 0) != (db < 0):
            result.append(a + (b - a) * da / (da - db))
    return result


def polygon_area(poly):
    if len(poly) < 3:
        return 0.0
    return sum(np.linalg.norm(np.cross(poly[i] - poly[0], poly[i+1] - poly[0])) / 2
               for i in range(1, len(poly) - 1))


def clipped_face_area(vertices, hulls):
    area = 0.0
    for tetra in hulls:
        poly = vertices
        for plane in tetra.equations:
            poly = clip_polygon(poly, plane)
            if len(poly) < 3:
                break
        area += polygon_area(poly)
    return area


def main():
    points, kernel, radius = cutter()
    source = brush(8219)
    if not convex(source["faces"], inward=True)["convex"]:
        raise ValueError("UID8219 is not the expected convex cavity")
    solid = np.unique(np.array([p for face in source["faces"] for p in face["points"]]), axis=0)
    obstacle = ConvexHull(solid)
    margins, witnesses, hulls = [], [], []
    for i in range(0, len(points), 3):
        tetra = ConvexHull(np.vstack((kernel, points[i:i+3])))
        hulls.append(tetra)
        planes = np.vstack((obstacle.equations, tetra.equations))
        optimum = linprog([0, 0, 0, -1], A_ub=np.column_stack((planes[:, :3],
                           np.ones(len(planes)))), b_ub=-planes[:, 3],
                           bounds=[(None, None)] * 4, method="highs")
        if not optimum.success:
            raise ValueError(f"intersection solve failed at triangle {i//3}: {optimum.message}")
        margins.append(float(optimum.x[3]))
        witnesses.append(optimum.x[:3].tolist())
    best = int(np.argmax(margins))
    source_words = {face["source_word"] for face in source["faces"]}
    compiled, _ = load("L1S2.rfl")
    affected = []
    for face in compiled:
        if face["source_word"] not in source_words:
            continue
        vertices = [np.array(p) for p in face["points"]]
        area = clipped_face_area(vertices, hulls)
        if area > 1e-5:
            affected.append({"face": face["id"], "room": face["room"]})
    # The room-8 second owner is not the whole overlap story: nearby detail
    # brushes can live in other compiled rooms but still occupy cutter space.
    other_words = {}
    cutter_lo, cutter_hi = points.min(axis=0), points.max(axis=0)
    for record in brushes():
        if record["uid"] in (8123, 8219):
            continue
        vertices = np.array([p for face in record["faces"] for p in face["points"]])
        if np.any(cutter_hi < vertices.min(axis=0)) or np.any(cutter_lo > vertices.max(axis=0)):
            continue
        for face in record["faces"]:
            other_words[face["source_word"]] = record["uid"]
    other_affected = []
    for face in compiled:
        uid = other_words.get(face["source_word"])
        if uid is None:
            continue
        area = clipped_face_area([np.array(p) for p in face["points"]], hulls)
        if area > 1e-5:
            other_affected.append({"uid": uid, "face": face["id"],
                                   "room": face["room"], "area": area})
    result = {"brush_uid": 8219, "triangles": len(margins), "template_radius": radius,
              "cutter_box": [points.min(axis=0).tolist(), points.max(axis=0).tolist()],
              "brush_box": [solid.min(axis=0).tolist(), solid.max(axis=0).tolist()],
              "maximum_intersection_margin": max(margins),
              "witness": witnesses[best],
              "intersecting_triangles": [i for i, margin in enumerate(margins) if margin > 1e-4],
              "affected_compiled_faces": affected,
              "other_affected_compiled_faces": other_affected}
    if max(margins) <= 0.05 or {(row["face"], row["room"]) for row in affected} != {(5780, 8), (5784, 8)}:
        raise AssertionError("The measured Driller/UID8219 intersection changed")
    if {(row["uid"], row["face"], row["room"]) for row in other_affected} != {
            (9996, 4972, 121), (9996, 4984, 121),
            (9996, 4985, 121), (9996, 4998, 121)}:
        raise AssertionError("The measured Driller/detail-brush intersection changed")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
