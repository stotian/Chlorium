import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'R4-72'))
from decode_checks import fixture
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_geometry import HEADER,SPRITE,META,decode,decode_origins
import json
from analyze_trace import STATE

def geometry():
    root=fixture(magic=b'CHLMIX74',version=74)
    header=HEADER.pack(b'CHLGEO75',75,368,2,2,1,0,2,0,0x100000,9,7,74.)
    source=[0]*45;source[0]=source[28]=0x200000;source[2]=source[29]=19;source[4]=1536;source[5]=1
    state=STATE.pack(1,2,0,0,7,96,0,0,0,0)
    rows=b''.join(state+SPRITE.pack(*source)+bytes(96)+META.pack(frame,0x1fa0,0,0,1,2) for frame in (1,2))
    return header+rows,root

class GeometryChecks(unittest.TestCase):
    def test_bounded_origin_phases(self):
        _,root=geometry();data=dict(diagnostic_only=True,bounded_first_gap_per_actor_frame=True,phases={'0':'other','1':'processFollowActions','2':'processTransformActions'},witnesses=[dict(uid=19,object_id=1,frame=1,root_row_ordinal=1,phase=1)])
        self.assertEqual(decode_origins(json.dumps(data).encode(),root)['phase_counts'],{'processFollowActions':1})
        for key,value in (('uid',999),('frame',3),('root_row_ordinal',999),('phase',3)):
            bad=json.loads(json.dumps(data));bad['witnesses'][0][key]=value
            with self.assertRaises(ValueError):decode_origins(json.dumps(bad).encode(),root)
        data['witnesses']*=2
        with self.assertRaises(ValueError):decode_origins(json.dumps(data).encode(),root)
    def test_linked_frame_witnesses(self):
        raw,root=geometry();self.assertEqual(decode(raw,root)['matched'],2)
    def test_malformed_and_foreign_identity(self):
        raw,root=geometry()
        for bad in (b'',raw[:-1],raw+b'x'):
            with self.assertRaises(ValueError):decode(bad,root)
        raw=bytearray(raw);raw[40:48]=(0x300000).to_bytes(8,'little')
        with self.assertRaises(ValueError):decode(raw,root)
    def test_unknown_metadata(self):
        raw,root=geometry();raw=bytearray(raw);raw[64+336:64+368]=META.pack(1,0x1fa0,0,1,1,2)
        with self.assertRaises(ValueError):decode(raw,root)

if __name__=='__main__':unittest.main()
