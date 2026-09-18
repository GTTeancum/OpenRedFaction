"""Controls for method decoding and draw-time state retention."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from xemu_draw_audit import decode_draws


def packed(*values):
    return struct.pack('<'+'I'*len(values),*values)


class DrawAuditTests(unittest.TestCase):
    def test_incrementing_bindings_and_repeated_draw_method(self):
        data = packed((2 << 18)|0x1b00,123,456,
                      (1 << 18)|0x17fc,5,
                      0x40000000|(2 << 18)|0x1810,0x02000006,0x05000009,
                      (1 << 18)|0x1b00,789,
                      (1 << 18)|0x1810,0x0200000f)
        draws = decode_draws(data)
        self.assertEqual([(d['start'],d['count']) for d in draws],[(6,3),(9,6),(15,3)])
        self.assertEqual([d['state'][0x1b00] for d in draws],[123,123,789])
        self.assertTrue(all(d['state'][0x1b04] == 456 for d in draws))

    def test_other_subchannel_does_not_change_3d_state(self):
        data = packed((1 << 18)|0x1b00,123,
                      (1 << 18)|(2 << 13)|0x1b00,999,
                      (1 << 18)|0x1810,0x02000000)
        self.assertEqual(decode_draws(data)[0]['state'][0x1b00],123)

    def test_rejects_truncation_jump_and_call(self):
        for data in (b'x',packed((2 << 18)|0x1b00,123),packed(1),packed(2)):
            with self.assertRaises(ValueError):
                decode_draws(data)


if __name__ == '__main__':
    unittest.main()
