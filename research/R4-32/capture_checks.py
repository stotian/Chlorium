import struct
from unittest import TestCase, main
import analyze_capture as capture


def blob(second_quad=None, second_owner=None, reason=0):
    result = bytearray(capture.FILE.pack(b'CHLPASS3', 2, 96, capture.SPRITE.size, 96))
    for frame in (1, 2):
        percent = struct.unpack('<Q', struct.pack('<d', 74.5))[0]
        result.extend(capture.FRAME.pack(frame, percent, 1, 1, 1, reason, 100, 200))
        result.extend(capture.DRAW.pack(1,2,3,4, 11,22,33,0,0,0,1,0,0,1,0,1,0,1,1,0))
        result.extend(second_quad if frame == 2 and second_quad is not None else bytes(96))
        sprite = [10,20,30,0,0,1] + [0]*39
        if frame == 2 and second_owner is not None:
            sprite[0] = second_owner
        result.extend(capture.SPRITE.pack(*sprite))
    return bytes(result)


class CaptureChecks(TestCase):
    def test_bitwise_classification_and_owner_identity(self):
        result = capture.summarize(blob())
        self.assertTrue(result['two_frame_context_valid'])
        self.assertEqual(result['identical_quads'], 1)
        self.assertEqual(result['same_mapped_owner'], 1)
        self.assertFalse(result['replacement_ready'])
        self.assertFalse(result['fps_gain_proven'])
        position = bytearray(96); position[0] = 1
        result = capture.summarize(blob(second_quad=position))
        self.assertEqual(result['position_changed'], 1)
        self.assertEqual(result['color_changed'], 0)
        result = capture.summarize(blob(second_owner=999))
        self.assertEqual(result['same_mapped_owner'], 0)

    def test_partial_or_different_frame_is_not_comparable(self):
        self.assertFalse(capture.summarize(blob(reason=1))['two_frame_context_valid'])

    def test_v1_is_readable_but_cannot_prove_missing_branch_inputs(self):
        result = bytearray(capture.FILE.pack(b'CHLPASS1',1,96,104,96))
        percent = struct.unpack('<Q',struct.pack('<d',74.5))[0]
        result.extend(capture.FRAME.pack(1,percent,1,1,1,0,100,200))
        result.extend(capture.DRAW.pack(1,2,3,4,11,22,33,0,0,0,1,0,0,1,0,1,0,1,1,0))
        result.extend(bytes(96))
        result.extend(capture.SPRITE_V1.pack(10,20,30,0,0,1,*([0]*18)))
        summary = capture.summarize(result)
        self.assertEqual(summary['capture_versions'],[1])
        self.assertFalse(summary['branch_inputs_complete'])
        self.assertFalse(summary['replacement_ready'])

    def test_resource_identity_survives_atlas_growth_without_sorting(self):
        data = bytearray(blob())
        # Add a new sprite before the retained sprite in the second draw.
        second = capture.FILE.size + capture.FRAME.size + capture.DRAW.size + 96 + capture.SPRITE.size
        prefix = data[:second]
        percent = struct.unpack('<Q', struct.pack('<d', 74.6))[0]
        prefix.extend(capture.FRAME.pack(2,percent,1,2,2,0,100,200))
        prefix.extend(capture.DRAW.pack(1,2,3,4,11,22,33,0,0,0,2,0,0,2,0,1,0,2,2,0))
        prefix.extend(bytes(192))
        prefix.extend(capture.SPRITE.pack(999,20,999,0,0,1,*([0]*39)))
        prefix.extend(capture.SPRITE.pack(10,20,30,1,0,1,*([0]*39)))
        result = capture.summarize(prefix)
        self.assertEqual(result['matched_draws'], 0)
        aligned = result['owner_resource_alignment']
        self.assertEqual(aligned['retained_owner_resources'], 1)
        self.assertEqual(aligned['added_owner_resources'], 1)
        self.assertFalse(aligned['retained_owner_order_changed'])

    def test_truncation_and_trailing_bytes(self):
        data = blob()
        for size in (0, 23, 24, 71, len(data)-1):
            with self.subTest(size=size), self.assertRaises(ValueError):
                capture.decode(data[:size])
        with self.assertRaises(ValueError):
            capture.decode(data + b'x')

    def test_bad_layout_bounds_and_ownership_cannot_fabricate_coverage(self):
        for offset, value in ((8, 3), (12, 100), (24+16, 2049),
                              (24+20, 65537), (24+48+4*8+13*4, 0)):
            data = bytearray(blob()); struct.pack_into('<I', data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                capture.decode(data)

    def test_autos_allowlists_agree_including_society(self):
        import re
        from pathlib import Path
        root = Path(__file__).resolve().parents[2]
        expected = {133260800, 129598490, 88952972, 139648964}
        source = (root / 'tests/auto_level_benchmark.cpp').read_text()
        authorized = re.search(r'authorizedLevels\{([^}]+)\}', source).group(1)
        self.assertEqual(set(map(int, re.findall(r'\d+', authorized))), expected)
        for name in ('research/R4-04/run-protected-test.ps1', 'tools/run-runtime-audit.ps1'):
            source = (root / name).read_text()
            allowed = re.search(r'\[ValidateSet\(([^)]+)\)\]\[int\]\$Level', source).group(1)
            self.assertEqual(set(map(int, allowed.split(','))), expected)


if __name__ == '__main__':
    main()
