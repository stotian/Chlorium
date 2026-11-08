"""Unexecuted regression cases while the user's test-stop instruction is active."""
import struct
from types import SimpleNamespace as NS
import unittest
from pe_ranges import function_ranges


def fixture():
    image = bytearray(0x400)
    image[0x300:0x304] = bytes((1, 0, 0, 0))
    image[0x304:0x308] = bytes((1 | (4 << 3), 0, 3, 0))
    struct.pack_into("<III", image, 0x310, 0x100, 0x120, 0x300)
    struct.pack_into("<III", image, 0x340, 0x100, 0x120, 0x300)
    records = [(0x100, 0x120, 0x300), (0x120, 0x180, 0x304), (0x180, 0x1A0, 0x341)]
    pe = NS(FILE_HEADER=NS(Machine=0x8664),
            DIRECTORY_ENTRY_EXCEPTION=[NS(struct=NS(BeginAddress=a, EndAddress=b, UnwindData=u)) for a, b, u in records],
            get_memory_mapped_image=lambda: bytes(image))
    return pe, image


class UnwindOwnership(unittest.TestCase):
    def test_split_body_and_indirect_share_the_real_owner(self):
        pe, _ = fixture()
        self.assertEqual(function_ranges(pe), [(0x100, 0x120, 0x100), (0x120, 0x180, 0x100), (0x180, 0x1A0, 0x100)])

    def test_cycle_rejects_instead_of_hanging(self):
        pe, image = fixture()
        struct.pack_into("<III", image, 0x310, 0x120, 0x180, 0x304)
        with self.assertRaisesRegex(ValueError, "cyclic"):
            function_ranges(pe)

    def test_truncated_chain_is_not_a_valid_procedure(self):
        pe, _ = fixture()
        pe.DIRECTORY_ENTRY_EXCEPTION[1].struct.UnwindData = 0x3FD
        with self.assertRaisesRegex(ValueError, "outside image"):
            function_ranges(pe)

    def test_conflicting_handler_flags_reject(self):
        pe, image = fixture()
        image[0x304] = 1 | (5 << 3)
        with self.assertRaisesRegex(ValueError, "conflicting"):
            function_ranges(pe)

    def test_truncated_root_codes_are_not_a_valid_owner(self):
        pe, image = fixture()
        image[0x3FC:0x400] = bytes((1, 0, 1, 0))
        pe.DIRECTORY_ENTRY_EXCEPTION[0].struct.UnwindData = 0x3FC
        with self.assertRaisesRegex(ValueError, "outside image"):
            function_ranges(pe)

    def test_truncated_root_handler_is_not_a_valid_owner(self):
        pe, image = fixture()
        image[0x3FC:0x400] = bytes((1 | (1 << 3), 0, 0, 0))
        pe.DIRECTORY_ENTRY_EXCEPTION[0].struct.UnwindData = 0x3FC
        with self.assertRaisesRegex(ValueError, "outside image"):
            function_ranges(pe)

    def test_future_format_is_not_guessed(self):
        pe, image = fixture()
        image[0x300] = 3
        with self.assertRaisesRegex(ValueError, "Unsupported"):
            function_ranges(pe)

    def test_overlapping_ranges_reject(self):
        pe, _ = fixture()
        pe.DIRECTORY_ENTRY_EXCEPTION[2].struct.BeginAddress = 0x170
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            function_ranges(pe)


if __name__ == "__main__":
    unittest.main()
