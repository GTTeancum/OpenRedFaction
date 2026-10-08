"""Pure synthetic tests of the telemetry validator, not Xbox/runtime evidence."""
import math
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import xemu_fighter_player_pursuit as pursuit


def bits(values):
    return list(struct.unpack('<'+'I'*len(values), struct.pack('<'+'f'*len(values), *values)))


def sample_history():
    recipe = {'authored_point': [0., 0., 10.]}
    rows = []
    for frame in range(pursuit.FRAMES-1):
        angle = frame*.001
        pose = [0., 0., frame*.1, math.cos(angle), 0., -math.sin(angle),
            0., 1., 0., math.sin(angle), 0., math.cos(angle)]
        player = [10.+frame*.05, 0., 50.]
        trace = [0]*32
        if frame:
            prior = rows[-1]
            position, basis, target = pursuit.vectors(prior[:3]), pursuit.vectors(prior[3:12]), pursuit.vectors(prior[12:15])
            throttle, rise, yaw = pursuit.steer(position, basis, target)
            trace = [frame, pursuit.OWNER, 123, 456, frame-1, 1, 2, 1] + bits(recipe['authored_point']) + \
                prior[12:15] + bits([throttle, 0., rise, yaw, 0.]) + [1] + prior[:12]
        rows.append(bits(pose+player) + trace + [frame])
    rows.append(rows[-1].copy())
    return {'extra': {pursuit.HISTORY: [word for row in rows for word in row]}}, recipe


class FighterPursuitHarnessTests(unittest.TestCase):
    def test_accepts_consistent_synthetic_trace(self):
        guest, recipe = sample_history()
        result = pursuit.check_motion(guest, recipe, 1)
        self.assertEqual(result['actual_commands'], 118)
        self.assertGreater(result['player_motion_response_frames'], 10)
        self.assertGreater(result['actual_turn_response_frames'], 3)

    def test_rejects_stale_sample(self):
        guest, recipe = sample_history()
        guest['extra'][pursuit.HISTORY][40*48+15+11] = bits([10.])[0]
        with self.assertRaisesRegex(RuntimeError, 'current player/start pose'):
            pursuit.check_motion(guest, recipe, 1)

    def test_rejects_wrong_actual_steering(self):
        guest, recipe = sample_history()
        guest['extra'][pursuit.HISTORY][40*48+15+17] = bits([-.5])[0]
        with self.assertRaisesRegex(RuntimeError, 'independently computed'):
            pursuit.check_motion(guest, recipe, 1)

    def test_rejects_presentation_step(self):
        guest, recipe = sample_history()
        guest['extra'][pursuit.HISTORY][-1] += 1
        with self.assertRaisesRegex(RuntimeError, 'Presentation-only'):
            pursuit.check_motion(guest, recipe, 1)

    def test_replay_is_bounded_guest_movement_only(self):
        for phase in (1, 2):
            raw = pursuit.replay(phase)
            self.assertEqual(raw[:8], b'RFI6'+struct.pack('<I', 48))
            self.assertEqual(len(raw), 8+120*48)
            active = []
            for frame in range(120):
                row = struct.unpack_from('<5f7I', raw, 8+frame*48)
                self.assertFalse(any(row[1:]))
                if row[0]:
                    active.append(frame)
                    self.assertEqual(row[0], 1 if phase == 1 else -1)
            self.assertEqual(active, list(range(20, 78) if phase == 1 else range(5, 40)))


if __name__ == '__main__':
    unittest.main()
