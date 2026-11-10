"""Extract the pinned native Follow iteration for owned tests only, not distribution."""
import hashlib
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'build-research/python'))
import pefile
import capstone
raw=Path('C:/Users/stoti/Chlorium/test-game/GeometryDash.exe').read_bytes()
if hashlib.sha256(raw).hexdigest()!='fc5a16c292278bc2e8e078fb1d5023c2bd658322dd72712767ea70c2dd9ec6d0':raise ValueError('Unqualified native image')
image=pefile.PE(data=raw).get_memory_mapped_image();start,end=0x23090f,0x2309b5;code=image[start:end]
if hashlib.sha256(code).hexdigest()!='2dd6f1e0c39ca2019ac14789194b93008e959ac9fe412db9ad08ae19fe98a051':raise ValueError('Native Follow fragment differs')
decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);decoder.detail=True
instructions=list(decoder.disasm(code,start))
if sum(i.size for i in instructions)!=len(code):raise ValueError('Incomplete native Follow decoding')
for i in instructions:
    for operand in i.operands:
        if operand.type==capstone.x86.X86_OP_MEM and operand.mem.base==capstone.x86.X86_REG_RIP:raise ValueError('Unrelocated native memory reference')
    if i.mnemonic.startswith('j') and (i.operands[0].type!=capstone.x86.X86_OP_IMM or not start<=i.operands[0].imm<end):raise ValueError('Native branch escapes Follow iteration')
calls=[i for i in instructions if i.mnemonic=='call']
if len(calls)!=1 or calls[0].address!=0x2309af or calls[0].op_str!='qword ptr [rax + 0x490]':raise ValueError('Unexpected native Follow callback')
out=ROOT/'build-research/follow-program-r76';out.mkdir(parents=True,exist_ok=True)
(out/'native_follow_fragment.inc').write_text('\n'.join('db '+','.join('0%02xh'%b for b in code[i:i+16]) for i in range(0,len(code),16))+'\n',encoding='ascii')
print('Pinned',len(code),'byte Follow iteration SHA256',hashlib.sha256(code).hexdigest(),'; no relocations')
