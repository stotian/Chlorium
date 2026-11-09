import struct
import unittest
from compile_trace import compile_packet, advance, PACKET, FRAME
from analyze_trace import HEADER, ROW, OBJECT, STATE


def synthetic_trace():
    states = [STATE.pack(.1+i*.005, .25-i*.002, 0, 0, 0, (i%2)|(2 if i%7==1 else 0)) for i in range(80)]
    states[0] = STATE.pack(float.fromhex('0x1.0000000000001p-32'), .25, 0, 0, 0, 0)
    records, rows = [], []
    bits = lambda x: struct.unpack('<Q', struct.pack('<d', x))[0]
    def record(i, state):return OBJECT.pack(1000+i*16, 7+i, 2, *STATE.unpack(state), 5, 1, 512, 1)
    for frame in (1, 2):
        for i in range(80):
            dx, dy = (.001, -.00025) if i else (-2**-32 if frame==1 else 0., 0.)
            stamp = (frame-1)*80+i+1
            repeat = 2 if i==2 else 1
            first = len(records)
            records.extend([record(i, states[i])]*repeat)
            rows.append(ROW.pack(44+i*16, bits(dx), bits(dy), frame, i+1, stamp, repeat, first, repeat, 1, 0x1f80))
            for _ in range(repeat):states[i] = advance(states[i], dx, dy, stamp, 0x1f80)
        first = len(records)
        for i,state in enumerate(states):
            values = list(STATE.unpack(state));values[5] &= 3;states[i] = STATE.pack(*values)
            records.append(record(i, states[i]))
        rows.append(ROW.pack(9000, 0, 0, frame, 1, 0, 80, first, 80, 33, 0x1f80))
    header = HEADER.pack(b'CHLGTR63', 63, 2, len(rows), len(records), 56, 64, 0, 1, 33, 8, 0, 74.)
    return header + b''.join(rows) + b''.join(records)


class Checks(unittest.TestCase):
    def test_cancellation_stays_on_the_exact_group_lattice(self):
        seed=STATE.pack(float.fromhex('0x1.0000000000001p-32'), .25, 0, 0, 0, 0)
        tiny=advance(seed,-2**-32,0.,1,0x1f80)
        self.assertEqual(STATE.unpack(tiny)[0],2**-84)
        self.assertIsNotNone(advance(tiny,0.,0.,2,0x1f80))
        outside=STATE.pack(float.fromhex('0x1.0000000000001p-84'), .25, 0, 0, 0, 0)
        self.assertIsNone(advance(outside,0.,0.,1,0x1f80))
    def test_shared_frame_program_keeps_global_table_and_duplicates(self):
        packet, report = compile_packet(synthetic_trace())
        self.assertEqual(report['selected_root_objects'], 80)
        self.assertEqual(report['joint_program_classes'], 80)
        self.assertEqual([frame['commands'] for frame in report['frames']], [80, 80])
        self.assertEqual([frame['max_steps'] for frame in report['frames']], [2, 2])
        self.assertEqual(PACKET.unpack_from(packet)[1:5], (64, 80, 80, 2))
        self.assertFalse(report['native_reader_closure_proven'])
        self.assertFalse(report['game_fps_evidence'])

    def test_truncated_capture_cannot_become_a_gpu_program(self):
        raw = bytearray(synthetic_trace());struct.pack_into('<I', raw, 32, 1)
        with self.assertRaisesRegex(ValueError, 'Complete trace'):compile_packet(raw)

    def test_changed_pose_is_excluded_without_repairing_it(self):
        raw = bytearray(synthetic_trace())
        header = HEADER.unpack_from(raw);object_start = HEADER.size + header[3]*ROW.size
        # First terminal observation of actor0 follows81 pre-call records.
        struct.pack_into('<d', raw, object_start + 81*OBJECT.size + 16, 17.)
        _, report = compile_packet(raw)
        self.assertEqual(report['selected_root_objects'], 79)

    def test_unsupported_reader_role_is_not_admitted(self):
        raw = bytearray(synthetic_trace());header = HEADER.unpack_from(raw)
        object_start = HEADER.size + header[3]*ROW.size
        struct.pack_into('<I', raw, object_start + 56, 512|1)
        _, report = compile_packet(raw)
        self.assertEqual(report['selected_root_objects'], 79)


if __name__ == '__main__':
    unittest.main()
