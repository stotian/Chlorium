"""Find six-vector quad-copy patterns in the existing static access inventory."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
from native_access_inventory import ROOT, below_normal_priority
from sprite_alias_inventory import full_register
import capstone
from capstone import x86_const as x86
import pefile

MOVES = {x86.X86_INS_MOVUPS,x86.X86_INS_MOVAPS,x86.X86_INS_MOVDQU,x86.X86_INS_MOVDQA}
OFFSETS = {0x1cc+16*n:n for n in range(6)}


def copy_pattern(instructions, start):
    first = instructions[start]
    if first.id not in MOVES or len(first.operands)!=2:
        return None
    source = first.operands[1]
    if source.type!=capstone.CS_OP_MEM or source.size!=16 or source.mem.index or source.mem.disp!=0x1cc:
        return None
    base = source.mem.base
    if not base or base==x86.X86_REG_RIP:
        return None
    vectors, reads, stores = {}, {}, {}
    destination = None
    for i in instructions[start:start+40]:
        if i.group(capstone.CS_GRP_CALL) or i.group(capstone.CS_GRP_JUMP) or i.group(capstone.CS_GRP_RET):
            break
        _, written = i.regs_access()
        changed={full_register(i.reg_name(register)) for register in written}
        if first.reg_name(base) in changed or (destination and i.reg_name(destination[0]) in changed):
            break
        previous = dict(vectors)
        for register in written:
            vectors.pop(register,None)
        if i.id not in MOVES or len(i.operands)!=2:
            continue
        left,right = i.operands
        if left.type==capstone.CS_OP_REG and right.type==capstone.CS_OP_MEM and right.size==16:
            if right.mem.base==base and not right.mem.index and right.mem.disp in OFFSETS:
                block = OFFSETS[right.mem.disp]
                vectors[left.reg]=block;reads[block]=hex(i.address)
        elif left.type==capstone.CS_OP_REG and right.type==capstone.CS_OP_REG and right.reg in previous:
            vectors[left.reg]=previous[right.reg]
        elif left.type==capstone.CS_OP_MEM and left.size==16 and right.type==capstone.CS_OP_REG and right.reg in previous:
            block=previous[right.reg]
            if block!=len(stores):
                return None
            if left.mem.index or not left.mem.base:
                return None
            proposed=(left.mem.base,left.mem.disp-block*16)
            if destination is None:
                destination=proposed
            if proposed!=destination:
                return None
            stores[block]=hex(i.address)
        if len(reads)==len(stores)==6:
            return dict(begin=hex(first.address),end=hex(i.address+i.size),source_base=first.reg_name(base),
                        destination_base=i.reg_name(destination[0]),destination_offset=hex(destination[1]),
                        vector_reads=reads,vector_stores=stores,
                        classification='six_forward_vector_copy_pattern_not_type_or_reader_closure_proof')
    return None


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory',type=Path)
    parser.add_argument('image',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    below_normal_priority()
    meta=json.loads((args.inventory/'inventory.local.json').read_text())
    module=next(m for m in meta['modules'] if m['module'].lower()==args.image.name.lower())
    raw=args.image.read_bytes()
    if len(raw)>64*1024*1024 or hashlib.sha256(raw).hexdigest()!=module['sha256']:
        raise ValueError('Image differs from inventoried file')
    owners=json.loads((args.inventory/(module['module']+'.owners.local.json')).read_text())
    starts={}
    with (args.inventory/(module['module']+'.accesses.csv')).open(encoding='utf-8') as stream:
        for row in csv.DictReader(stream):
            if row['field']=='sprite_quad' and row['access']=='read' and row['width']=='16' and row['displacement']=='0x1cc':
                starts.setdefault(row['owner'],set()).add(int(row['instruction'],16))
    pe=pefile.PE(data=raw,fast_load=True)
    try:
        image=pe.get_memory_mapped_image()
        decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);decoder.detail=True
        found=[]
        for owner,addresses in starts.items():
            for begin,end in owners[owner]['ranges']:
                begin,end=int(begin,16),int(end,16)
                instructions=list(decoder.disasm(image[begin:end],begin))
                for n,i in enumerate(instructions):
                    if i.address in addresses:
                        pattern=copy_pattern(instructions,n)
                        if pattern:
                            found.append(dict(pattern,owner=owner,exports=owners[owner]['names']))
        result=dict(static_analysis_only=True,target_code_executed=False,all_cpu_consumers_proven=False,
                    module=module['module'],module_sha256=module['sha256'],patterns=found,
                    limitations=['Only the explicit six16-byte load/store form is recognized',
                                 'Other structures may share this layout; other copy forms remain open'])
        with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2)
        print(json.dumps(dict(module=module['module'],candidate_owners=len(starts),patterns=len(found))))
    finally:
        pe.close()


if __name__=='__main__':main()
