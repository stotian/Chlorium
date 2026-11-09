"""Extract a pinned owned native arithmetic fragment; never publish GD instructions."""
import hashlib
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'build-research/python'))
import pefile
raw=Path('C:/Users/stoti/Chlorium/test-game/GeometryDash.exe').read_bytes()
if hashlib.sha256(raw).hexdigest()!='fc5a16c292278bc2e8e078fb1d5023c2bd658322dd72712767ea70c2dd9ec6d0':raise ValueError('Unqualified native game image')
image=pefile.PE(data=raw).get_memory_mapped_image();code=image[0x22bd8d:0x22bed4]
if hashlib.sha256(code).hexdigest()!='b274a277ed00667d267f01979a45b8aa605fb7510e18a97634de8262ab63fd13':raise ValueError('Rotation fragment changed')
parts=[];cursor=0
for site,slot in [(0x22be4b,'r71PointCtor'),(0x22be5f,'r71PointApply')]:
    at=site-0x22bd8d
    if code[at:at+2]!=b'\xff\x15':raise ValueError('Native import call shape changed')
    for i in range(cursor,at,16):parts.append('db '+','.join('0%02xh'%b for b in code[i:min(i+16,at)]))
    parts.append('call qword ptr ['+slot+']');cursor=at+6
for i in range(cursor,len(code),16):parts.append('db '+','.join('0%02xh'%b for b in code[i:i+16]))
out=ROOT/'build-research/mixed-program-r71';out.mkdir(parents=True,exist_ok=True)
(out/'native_rotation_fragment.inc').write_text('\n'.join(parts)+'\n',encoding='ascii')
print('Pinned 327-byte native root-rotation arithmetic fragment; two import relocations only')
