import json
from pathlib import Path
import tempfile
import unittest
import resolve_reads as r


class Reads(unittest.TestCase):
    def test_instruction_direction_and_exact_end(self):
        decoder = r.capstone.Cs(r.capstone.CS_ARCH_X86, r.capstone.CS_MODE_64)
        decoder.detail = True
        image = b'\x90' * 16 + bytes.fromhex('8b018901c3')
        ranges = [(16, 21, 16)]
        read = r.resolve_instruction(image, ranges, 18, image[3:18], decoder, {})
        self.assertEqual(read['access'], 'read')
        write = r.resolve_instruction(image, ranges, 20, image[5:20], decoder, {})
        self.assertEqual(write['access'], 'write')
        self.assertEqual(r.resolve_instruction(image, ranges, 17, image[2:17], decoder, {})['status'], 'not_instruction_end')
        self.assertEqual(r.resolve_instruction(image, ranges, 18, b'\0'*15, decoder, {})['status'], 'runtime_instruction_differs')

    def test_bad_input_cannot_claim_read_coverage(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'data.json'
            for value in ({}, {'diagnostic_only':True, 'fps_evidence':True, 'events':[]},
                          {'diagnostic_only':True, 'fps_evidence':False, 'events':[{'slots':16}]},
                          {'diagnostic_only':True, 'fps_evidence':False, 'events':[{'slots':1, 'preceding_bytes_at_finish':'ff'}]}):
                path.write_text(json.dumps(value))
                with self.assertRaises(ValueError):r.load_observation(path)

    def test_leaf_requires_export_bound_and_straight_line_return(self):
        decoder = r.capstone.Cs(r.capstone.CS_ARCH_X86, r.capstone.CS_MODE_64);decoder.detail = True
        image = b'\x90'*16 + bytes.fromhex('8b01c3cc')
        self.assertEqual(r.exported_leaf(image, 16, 20, decoder), (16, 19, 16))
        self.assertIsNone(r.exported_leaf(image, 16, 18, decoder))
        self.assertIsNone(r.exported_leaf(b'\x90'*16+bytes.fromhex('eb00c3'), 16, 19, decoder))


if __name__ == '__main__':unittest.main()
