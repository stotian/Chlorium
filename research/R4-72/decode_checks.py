import unittest
from analyze_trace import HEADER,STATE,COMMAND,TAIL,decode


def fixture(**changes):
    values=dict(magic=b'CHLROT72',version=72,row_bytes=208,frames=2,count=2,actors=1,reasons=0,uid=9,thread=7,owner=0x100000,percent=74.,calls=2);values.update(changes)
    state=STATE.pack(1,2,0,0,7,96,0,0,0,0);command=COMMAND.pack(0,0,*([0.]*12),7,1,0,0)
    return HEADER.pack(*values.values())+b''.join(state+command+state+TAIL.pack(0x200000,19,frame,1,0,0x1fa0,0) for frame in (1,2))


class DecodeChecks(unittest.TestCase):
    def test_completed_window(self):self.assertEqual(decode(fixture())['unique_commands'],1)
    def test_bad_headers(self):
        for change in [dict(version=71),dict(row_bytes=200),dict(count=65537),dict(actors=129),dict(reasons=2),dict(frames=1),dict(percent=float('nan'))]:
            with self.subTest(change=change),self.assertRaises(ValueError):decode(fixture(**change))
    def test_truncated_extra_and_missing_post(self):
        for raw in [b'',fixture()[:-1],fixture()+b'x']:
            with self.assertRaises(ValueError):decode(raw)
        raw=bytearray(fixture());at=HEADER.size+176;raw[at:at+TAIL.size]=TAIL.pack(0x200000,19,1,0,0,0x1fa0,0)
        with self.assertRaises(ValueError):decode(raw)


if __name__=='__main__':unittest.main()
