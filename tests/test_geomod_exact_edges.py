"""Independent topology-audit controls, including a point-touching false closure."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_geomod_exact_edges import audit


def tetra(offset=(0, 0, 0), height=1):
    points = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, height)]
    points = [tuple(a+b for a, b in zip(p, offset)) for p in points]
    return [[points[i] for i in f] for f in ((0, 2, 1), (0, 1, 3), (1, 2, 3), (2, 0, 3))]


def encode(faces):
    vertices = [v for face in faces for v in face]
    data = b'RGM1'+struct.pack('<II', len(vertices), len(faces))
    data += b''.join(struct.pack('<5f', *v, 0, 0) for v in vertices)
    first = 0
    for face in faces:
        data += struct.pack('<4I', first, len(face), 0, 0)
        first += len(face)
    return data


class ExactTopologyTests(unittest.TestCase):
    def test_closed_and_thin_shells(self):
        for height in (1, 1e-7):
            self.assertTrue(audit(encode(tetra(height=height)))['combinatorial_closed'])

    def test_missing_face_and_duplicate_shell(self):
        faces = tetra()
        for invalid in (faces[:-1], faces+faces):
            self.assertFalse(audit(encode(invalid))['combinatorial_closed'])

    def test_point_touch_requires_vertex_links(self):
        faces = tetra()
        other = [[tuple(-x for x in p) for p in f] for f in tetra()]
        result = audit(encode(faces+other))
        self.assertTrue(result['exact_pairs'])
        self.assertFalse(result['vertex_manifold'])
        self.assertEqual(len(result['nonmanifold_vertices']), 1)

    def test_disconnected_closed_shells(self):
        self.assertTrue(audit(encode(tetra()+tetra((3, 0, 0))))['combinatorial_closed'])

    def test_truncated_input_and_zero_edge(self):
        with self.assertRaises(ValueError):
            audit(encode(tetra())[:-1])
        faces = tetra()
        faces[0][1] = faces[0][0]
        self.assertFalse(audit(encode(faces))['combinatorial_closed'])


if __name__ == '__main__':
    unittest.main()
