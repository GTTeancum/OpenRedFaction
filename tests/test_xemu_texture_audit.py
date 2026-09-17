"""Independent small rectangular Morton-layout controls for memory readback."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from xemu_texture_audit import swizzle_rgba


class TextureAuditTests(unittest.TestCase):
    def test_rectangular_layouts(self):
        raw = b''.join(bytes([i])*4 for i in range(8))
        self.assertEqual(swizzle_rgba(raw,4,2),
                         b''.join(bytes([i])*4 for i in [0,1,4,5,2,3,6,7]))
        self.assertEqual(swizzle_rgba(raw,2,4),raw)

    def test_invalid_dimensions_and_size(self):
        for width,height,data in [(3,2,bytes(24)),(4,2,bytes(31)),(0,2,b'')]:
            with self.assertRaises(ValueError):
                swizzle_rgba(data,width,height)


if __name__ == '__main__':
    unittest.main()
