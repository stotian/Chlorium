import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'R4-72'))
from analyze_trace import HEADER,COMMAND,TAIL,decode
from decode_checks import fixture

def follow_fixture():
    raw=bytearray(fixture(magic=b'CHLMIX76',version=76))
    for frame,at in enumerate((64,272),1):
        raw[at+48:at+128]=COMMAND.pack(.125,-.25,*([0.]*12),7,2,3,0)
        raw[at+176:at+208]=TAIL.pack(0x200000,19,frame,1,0,0x1fa0,1)
    return raw

class FollowChecks(unittest.TestCase):
    def test_native_follow_family(self):
        self.assertEqual(decode(follow_fixture(),mixed=True)['follow_rows'],2)
        with self.assertRaises(ValueError):decode(follow_fixture())
    def test_masks_and_call_entry_provenance(self):
        for mask in (0,1,2,3):
            raw=follow_fixture();raw[64+48:64+128]=COMMAND.pack(.125,-.25,*([0.]*12),7,2,mask,0);self.assertEqual(decode(raw,mixed=True)['follow_rows'],2)
        for mask in (4,0xffffffff):
            raw=follow_fixture();raw[64+48:64+128]=COMMAND.pack(.125,-.25,*([0.]*12),7,2,mask,0)
            with self.assertRaises(ValueError):decode(raw,mixed=True)
        raw=follow_fixture();raw[64+176:64+208]=TAIL.pack(0x200000,19,1,1,0,0x1fa0,0)
        with self.assertRaises(ValueError):decode(raw,mixed=True)
    def test_legacy_format_cannot_hide_follow(self):
        raw=follow_fixture();raw[:8]=b'CHLMIX74';raw[8:12]=(74).to_bytes(4,'little')
        with self.assertRaises(ValueError):decode(raw,mixed=True)

if __name__=='__main__':unittest.main()
