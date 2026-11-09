import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'R4-72'))
from analyze_trace import HEADER,COMMAND,TAIL,decode
from decode_checks import fixture

class JointChecks(unittest.TestCase):
    def test_rotation_family(self):
        raw=fixture(magic=b'CHLMIX74',version=74);self.assertEqual(decode(raw,mixed=True)['rotation_rows'],2)
        with self.assertRaises(ValueError):decode(raw)
    def test_move_source_and_duplicate(self):
        raw=bytearray(fixture(magic=b'CHLMIX74',version=74))
        for frame,at in enumerate((64,272),1):
            raw[at+48:at+128]=COMMAND.pack(.125,0,*([0.]*12),7,0,0,0)
            raw[at+176:at+208]=TAIL.pack(0x200000,19,frame,1,0,0x1fa0,3)
        self.assertEqual(decode(raw,mixed=True)['movement_rows'],2)
        raw[64+176:64+208]=TAIL.pack(0x200000,19,1,1,0,0x1fa0,0)
        with self.assertRaises(ValueError):decode(raw,mixed=True)
    def test_unknown_source_bits_and_wrong_family(self):
        raw=bytearray(fixture(magic=b'CHLMIX74',version=74));raw[64+176:64+208]=TAIL.pack(0x200000,19,1,1,0,0x1fa0,1<<24)
        with self.assertRaises(ValueError):decode(raw,mixed=True)
        with self.assertRaises(ValueError):decode(fixture(),mixed=True)

if __name__=='__main__':unittest.main()
