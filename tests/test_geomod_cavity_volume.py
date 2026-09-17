"""Analytic controls for the independent cavity membership audit."""
from pathlib import Path
import sys
import unittest
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_geomod_cavity_volume import expected_membership, winding


class VolumeAuditTests(unittest.TestCase):
    def setUp(self):
        self.vertices = np.array([(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)], dtype=float)
        self.surface = self.vertices[np.array([(0, 2, 1), (0, 1, 3), (1, 2, 3), (2, 0, 3)])]
        self.points = np.array([(.1, .1, .1), (.5, .5, .5), (2.1, .1, .1), (-1, -1, -1)])

    def test_tetrahedron_winding_and_orientation(self):
        np.testing.assert_allclose(winding(self.points, self.surface), [1, 0, 0, 0], atol=1e-12)
        np.testing.assert_allclose(winding(self.points, self.surface[:, ::-1]), [-1, 0, 0, 0], atol=1e-12)

    def test_union_membership_uses_cutter(self):
        cutter = (self.vertices + [2, 0, 0])[None, :, :]
        np.testing.assert_array_equal(expected_membership(self.points, self.surface, cutter), [True, False, True, False])
        # The source alone lacks the new cavity and must disagree at point2.
        self.assertLess(abs(winding(self.points[2:3], self.surface)[0]), 1e-12)

    def test_duplicated_shell_has_winding_two(self):
        double = np.concatenate([self.surface, self.surface])
        self.assertAlmostEqual(winding(self.points[:1], double)[0], 2)


if __name__ == '__main__':
    unittest.main()
