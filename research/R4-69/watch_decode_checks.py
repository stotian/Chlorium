"""Malformed R69 traces must fail before PE reads or report publication."""
import unittest
from analyze_watch import HEADER, ACTOR, ROW, decode


def fixture(**changes):
    values = dict(magic=b'CHLRD69\0', version=69, row_size=32, actor_size=16, reasons=0, frames=2, count=2, objects=2,
                  thread=17, features=1, game=0x140000000, cocos=0x180000000, percent=74.)
    values.update(changes)
    return HEADER.pack(*values.values()) + ACTOR.pack(0x100000, 21, 1) + ACTOR.pack(0x200000, 22, 2) + ROW.pack(0x140100008, 0x140100008, 1, 1, 70) + ROW.pack(0x140100010, 0x140100010, 2, 8, 71)


class DecodeChecks(unittest.TestCase):
    def test_complete_selected_window(self):
        meta, rows = decode(fixture()); self.assertEqual(meta['frames'], [70, 71]); self.assertEqual(len(rows), 2)

    def test_bad_header_or_budget(self):
        for change in [dict(magic=b'wrong!!!'), dict(count=32769), dict(reasons=4), dict(objects=1), dict(features=0), dict(frames=1), dict(percent=float('nan'))]:
            with self.subTest(change=change), self.assertRaises(ValueError): decode(fixture(**change))

    def test_truncated_and_trailing_bytes(self):
        for raw in (b'', fixture()[:-1], fixture()+b'x'):
            with self.assertRaises(ValueError): decode(raw)

    def test_bad_identity_mask_or_frame(self):
        raw=fixture()
        for offset, payload in [(HEADER.size, ACTOR.pack(0x100001,21,1)), (HEADER.size+16, ACTOR.pack(0x100000,22,2)),
                                (HEADER.size+32, ROW.pack(1,1,1,16,70)), (HEADER.size+64, ROW.pack(1,1,1,8,70))]:
            with self.assertRaises(ValueError): decode(raw[:offset]+payload+raw[offset+len(payload):])


if __name__ == '__main__': unittest.main()
