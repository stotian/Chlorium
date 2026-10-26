"""Generate relocation metadata only; never export native instruction bytes."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"build-research/python"))
import capstone
import pefile

root=Path(__file__).resolve().parent
game=Path("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/GeometryDash.exe")
pe=pefile.PE(str(game));data=pe.get_memory_mapped_image()
assert pe.FILE_HEADER.TimeDateStamp==0x69702da0 and pe.OPTIONAL_HEADER.SizeOfImage==0xa3a000
imports={entry.address-pe.OPTIONAL_HEADER.ImageBase:entry.name.decode() for dll in pe.DIRECTORY_ENTRY_IMPORT
         if dll.dll.lower()==b"libcocos2d.dll" for entry in dll.imports if entry.name}
def fnv(body):
    h=14695981039346656037
    for b in body:h=((h^b)*1099511628211)&((1<<64)-1)
    return h
d=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);d.detail=True
bodies=[(0x229720,0x662,0,0xa6208a97dbf7526d),(0x197b20,0x33,0x1000,0xbd7ca077b47adc27)]
lines=["#pragma once", "// Generated relocation/identity metadata, no game instructions.",
       "namespace chlorium::research::transform_layout {",
       "struct Body {unsigned rva,length,offset;unsigned long long hash;};",
       "enum Kind {Data,Import,Reset,Move,Sqrt,Atan2,Cookie};",
       "struct Patch {unsigned site,displacement,next,kind,target,size;unsigned long long hash;const char* symbol;};",
       "inline constexpr Body bodies[]{"]
patches=[]
direct={0x227c30:"Reset",0x22aab0:"Move",0x4d188a:"Sqrt",0x4d1848:"Atan2",0x4d0750:"Cookie"}
for rva,length,out,expected in bodies:
    body=data[rva:rva+length];instructions=list(d.disasm(body,rva));starts={i.address for i in instructions}
    assert fnv(body)==expected,"unsupported native body fingerprint"
    assert sum(i.size for i in instructions)==length and instructions[-1].mnemonic=="ret"
    lines.append(f"    {{0x{rva:x},0x{length:x},0x{out:x},0x{fnv(body):016x}ull}},")
    for i in instructions:
        if i.group(capstone.CS_GRP_JUMP):assert i.operands[0].type==capstone.x86.X86_OP_IMM and i.operands[0].imm in starts
        if i.mnemonic=="call" and i.operands[0].type==capstone.x86.X86_OP_IMM:
            assert i.imm_size==4
            patches.append((out+i.address-rva,i.imm_offset,i.size,direct[i.operands[0].imm],i.operands[0].imm,0,0,""))
        for op in i.operands:
            if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
                assert i.disp_size==4
                target=i.address+i.size+op.mem.disp
                if target in imports:
                    patches.append((out+i.address-rva,i.disp_offset,i.size,"Import",target,8,0,imports[target]))
                else:
                    assert i.mnemonic!="call","unresolved native import"
                    assert op.size in (4,8,16)
                    patches.append((out+i.address-rva,i.disp_offset,i.size,"Data",target,op.size,fnv(data[target:target+op.size]),""))
lines.extend(["};","inline constexpr Patch patches[]{"])
for site,disp,n,kind,target,size,h,symbol in patches:
    lines.append(f'    {{0x{site:x},{disp},{n},{kind},0x{target:x},{size},0x{h:016x}ull,"{symbol}"}},')
lines.extend(["};","}",""])
cc=pefile.PE(str(game.parent/"libcocos2d.dll"));ccdata=cc.get_memory_mapped_image()
assert cc.FILE_HEADER.TimeDateStamp==0x69702d99 and cc.OPTIONAL_HEADER.SizeOfImage==0x1c5000
lines[-2:-2]=["struct CocosBody {unsigned rva,length,slot;unsigned long long hash;};","inline constexpr CocosBody cocosBodies[]{"]
cocos_bodies=[(0x4d70,0x1a,0xb8,0x9e334b49abde79b2),(0x4e90,0x14,0x160,0x082a7ac626f078e7),
              (0x4eb0,0x14,0x170,0x7055610577a704f3),(0x4f10,0x14,0x78,0x527665047f2eb17f),
              (0x4f30,0x14,0x88,0xbc45da021d44716b),(0x37f0,0x468,0x308,0x7abc292bc9db4855)]
for rva,n,slot,expected in cocos_bodies:
    assert fnv(ccdata[rva:rva+n])==expected,"unsupported Cocos helper body"
    lines.insert(-2,f"    {{0x{rva:x},0x{n:x},0x{slot:x},0x{expected:016x}ull}},")
lines.insert(-2,"};")
(root/"NativeLayout.hpp").write_text("\n".join(lines),encoding="utf-8",newline="\n")
print(f"PASS metadata: {len(bodies)} complete bodies, {len(patches)} explicit relocations, no instruction bytes exported")
