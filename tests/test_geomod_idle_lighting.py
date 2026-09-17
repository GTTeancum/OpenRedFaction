"""Negative controls for saved-atlas diagnostics, using retained live bytes."""
import csv
import io
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_geomod_idle_lighting import audit


def fixture(packed='29a5', extra=False):
    stream = io.StringIO()
    writer = csv.writer(stream)
    writer.writerow(['map', 'seed', 'width', 'height', 'atlas_x', 'atlas_y', 'image', 'faces', 'packed'])
    # First texel of the retained 16-cut atlas: seed1 -> 0xa529.
    writer.writerow([0, 1, 1, 1, 0, 0, 3, 1, packed])
    if extra:
        writer.writerow([1, 1, 1, 1, 0, 0, 3, 0, '29a5'])
    return stream.getvalue().encode('ascii')


class IdleLightingTests(unittest.TestCase):
    def test_retained_texel(self):
        self.assertEqual(audit(fixture())['result'], 'PASS')

    def test_corrupt_green_and_alpha_detected(self):
        for packed in ('09a5', '2925'):
            result = audit(fixture(packed))
            self.assertEqual(result['result'], 'DIFFERENT')
            self.assertEqual(result['mismatches'][0]['different_texels'], 1)

    def test_aliasing_rejected_even_unused(self):
        with self.assertRaisesRegex(ValueError, 'Overlapping'):
            audit(fixture(extra=True))

    def test_truncation_rejected(self):
        with self.assertRaisesRegex(ValueError, 'bytes'):
            audit(fixture('29'))


if __name__ == '__main__':
    unittest.main()
