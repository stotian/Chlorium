import struct
import unittest
from analyze_trace import HEADER, ROW, OBJECT, STATE, analyze, decode, predict


def record(address=100, state=None):
    state = STATE.pack(.25, -.5, 0, 0, 0, 0) if state is None else state
    return OBJECT.pack(address, 7, 2, *STATE.unpack(state), 5, 1, 512, 1)


def fixture(discontinuity=False, duplicate=False):
    before = STATE.pack(.25, -.5, 0, 0, 0, 0)
    moved = predict(before, .125, -.25, 9, 0x1f80)
    if duplicate:
        moved = predict(moved, .125, -.25, 9, 0x1f80)
    if discontinuity:
        state = list(STATE.unpack(moved));state[0] += 17;moved = STATE.pack(*state)
    data = record(state=before) * (2 if duplicate else 1) + record(state=moved)
    first_count = 2 if duplicate else 1
    header = HEADER.pack(b'CHLGTR63', 63, 2, 2, first_count + 1, 56, 64, 0, 0, 33, 8, 0, 74.)
    bits = lambda x: struct.unpack('<Q', struct.pack('<d', x))[0]
    rows = ROW.pack(44, bits(.125), bits(-.25), 1, 4, 9, first_count, 0, first_count, 1, 0x1f80)
    rows += ROW.pack(44, 0, 0, 2, 4, 10, 1, first_count, 1, 1, 0x1f80)
    return header + rows + data


class Checks(unittest.TestCase):
    def test_terminal_observation_is_not_a_move(self):
        before = STATE.pack(.25, -.5, 0, 0, 0, 0)
        moved = predict(before, .125, -.25, 9, 0x1f80)
        bits = lambda x: struct.unpack('<Q', struct.pack('<d', x))[0]
        header = HEADER.pack(b'CHLGTR63', 63, 2, 3, 3, 56, 64, 0, 1, 33, 8, 0, 74.)
        rows = ROW.pack(44, bits(.125), bits(-.25), 1, 4, 9, 1, 0, 1, 1, 0x1f80)
        rows += ROW.pack(55, 0, 0, 1, 5, 9, 5, 1, 1, 33, 0x1f80)
        rows += ROW.pack(55, 0, 0, 2, 5, 9, 5, 2, 1, 33, 0x1f80)
        result = analyze(header + rows + record(state=before) + record(state=moved) * 2)
        self.assertTrue(result['complete_observation'])
        self.assertEqual(result['unverified_final_states'], 0)
        self.assertEqual(result['observed_movement_pose_consistent_objects'], 1)
        self.assertEqual(result['member_visits'], 1)
        self.assertEqual(result['counts']['end_frame_snapshots'], 2)
        self.assertEqual(result['counts']['end_frame_position_matches'], 2)
        self.assertFalse(result['native_reader_closure_proven'])

    def test_incomplete_or_out_of_order_terminal_observation_rejects(self):
        header = HEADER.pack(b'CHLGTR63', 63, 2, 1, 0, 56, 64, 0, 1, 33, 8, 0, 74.)
        row = ROW.pack(55, 0, 0, 1, 5, 9, 0, 0, 0, 33, 0x1f80)
        with self.assertRaisesRegex(ValueError, 'Missing declared terminal'):
            decode(header + row)
    def test_continuity(self):
        result = analyze(fixture())
        self.assertEqual(result['counts']['full_state_matches'], 1)
        self.assertEqual(result['counts']['position_matches'], 1)
        self.assertEqual(result['unverified_final_states'], 1)
        self.assertTrue(result['complete_observation'])
        self.assertFalse(result['native_reader_closure_proven'])
        self.assertFalse(result['game_fps_evidence'])

    def test_external_writer_is_observed_not_called_a_game_bug(self):
        result = analyze(fixture(discontinuity=True))
        self.assertEqual(result['discontinuity_fields']['position'], 1)
        self.assertEqual(result['counts']['position_matches'], 0)

    def test_duplicate_members_execute_twice(self):
        result = analyze(fixture(duplicate=True))
        self.assertEqual(result['counts']['duplicate_member_visits'], 1)
        self.assertEqual(result['counts']['full_state_matches'], 1)

    def test_signed_zero_and_x_lock(self):
        state = STATE.pack(-0., .5, 0, 0, 1, 3)
        result = predict(state, 12, -0., 2, 0x1f80)
        self.assertEqual(result[:8], state[:8])
        self.assertEqual(STATE.unpack(result)[2:5], (0, 0, 1))

    def test_unknown_rounding_and_nonfinite_reject(self):
        state = STATE.pack(1., 1., 0, 0, 0, 0)
        self.assertIsNone(predict(state, 1., 0., 1, 0x3f80))
        self.assertIsNone(predict(state, float('inf'), 0., 1, 0x1f80))

    def test_malformed_bounds_reject(self):
        raw = fixture()
        for changed in [raw[:-1], raw + b'x', raw[:32], b'x' * len(raw)]:
            with self.assertRaises(ValueError):decode(changed)
        changed = bytearray(raw);struct.pack_into('<I', changed, 20, 262145)
        with self.assertRaises(ValueError):decode(changed)
        changed = bytearray(raw);struct.pack_into('<I', changed, 64 + 40, 99)
        with self.assertRaises(ValueError):decode(changed)

    def test_call_window_above_old_limit_is_complete(self):
        rows = ROW.pack(0, 0, 0, 1, 0, 0, 0, 0, 0, 17, 0x1f80) * 3000
        header = HEADER.pack(b'CHLGTR63', 63, 2, 3000, 0, 56, 64, 0, 0, 33, 8, 0, 74.)
        result = analyze(header + rows)
        self.assertEqual(result['calls'], 3000)
        self.assertTrue(result['complete_observation'])
        self.assertFalse(result['native_reader_closure_proven'])


if __name__ == '__main__':
    unittest.main()
