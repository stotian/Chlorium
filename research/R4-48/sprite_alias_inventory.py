"""Conservative local RCX-relative alias analysis in reviewed CCSprite exports."""
from collections import deque
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import sys

from native_access_inventory import ROOT, IDENTITIES, below_normal_priority
import capstone
from capstone import x86_const as x86
import pefile
from pe_ranges import function_ranges

VOLATILE = {'rax', 'rcx', 'rdx', 'r8', 'r9', 'r10', 'r11'}
ARGUMENTS = ('rcx', 'rdx', 'r8', 'r9')
QUAD = (0x1cc, 0x22c)


def full_register(name):
    aliases = {'al':'rax', 'ah':'rax', 'ax':'rax', 'eax':'rax',
               'bl':'rbx', 'bh':'rbx', 'bx':'rbx', 'ebx':'rbx',
               'cl':'rcx', 'ch':'rcx', 'cx':'rcx', 'ecx':'rcx',
               'dl':'rdx', 'dh':'rdx', 'dx':'rdx', 'edx':'rdx',
               'sil':'rsi', 'si':'rsi', 'esi':'rsi', 'dil':'rdi', 'di':'rdi', 'edi':'rdi',
               'spl':'rsp', 'sp':'rsp', 'esp':'rsp', 'bpl':'rbp', 'bp':'rbp', 'ebp':'rbp'}
    if name in aliases:
        return aliases[name]
    return re.sub(r'^(r(?:[89]|1[0-5]))[bwd]$', r'\1', name)


def address(instruction, operand, registers):
    base = registers.get(instruction.reg_name(operand.mem.base))
    if not base or base[0] not in ('sprite', 'stack'):
        return None
    index = ('constant', 0) if not operand.mem.index else registers.get(instruction.reg_name(operand.mem.index))
    if index is None or index[0] != 'constant':
        return None
    return base[0], base[1]+operand.mem.disp+index[1]*operand.mem.scale


def value(instruction, operand, registers, stack):
    if operand.type == capstone.CS_OP_IMM:
        return 'constant', operand.imm
    if operand.type == capstone.CS_OP_REG and operand.size == 8:
        return registers.get(instruction.reg_name(operand.reg))
    if operand.type == capstone.CS_OP_MEM and operand.size == 8:
        where = address(instruction, operand, registers)
        if where and where[0] == 'stack':
            return stack.get(where[1])
    return None


def transfer(instruction, state):
    registers, stack = dict(state[0]), dict(state[1])
    before = dict(registers)
    operands = instruction.operands
    _, written = instruction.regs_access()
    for register in written:
        registers.pop(full_register(instruction.reg_name(register)), None)
    for operand in operands:
        if operand.type == capstone.CS_OP_MEM and operand.access & capstone.CS_AC_WRITE:
            where = address(instruction, operand, before)
            if where and where[0] == 'stack':
                for slot in list(stack):
                    if slot < where[1]+operand.size and slot+8 > where[1]:
                        del stack[slot]
            elif not where:
                stack.clear()
    if instruction.id == x86.X86_INS_MOV and len(operands) == 2:
        dest, source = operands
        result = value(instruction, source, before, state[1])
        if dest.type == capstone.CS_OP_REG and dest.size == 8 and result:
            registers[instruction.reg_name(dest.reg)] = result
        elif dest.type == capstone.CS_OP_REG and dest.size == 4 and source.type == capstone.CS_OP_IMM:
            registers[full_register(instruction.reg_name(dest.reg))] = ('constant', source.imm & 0xffffffff)
        elif dest.type == capstone.CS_OP_MEM and dest.size == 8 and result:
            where = address(instruction, dest, before)
            if where and where[0] == 'stack':
                stack[where[1]] = result
    elif instruction.id == x86.X86_INS_LEA and len(operands) == 2 and operands[0].size == 8:
        result = address(instruction, operands[1], before)
        if result:
            registers[instruction.reg_name(operands[0].reg)] = result
    elif instruction.id in (x86.X86_INS_ADD, x86.X86_INS_SUB) and len(operands) == 2:
        dest, source = operands
        if dest.type == capstone.CS_OP_REG and dest.size == 8 and source.type == capstone.CS_OP_IMM:
            name = instruction.reg_name(dest.reg)
            if name in before:
                kind, offset = before[name]
                delta = source.imm if instruction.id == x86.X86_INS_ADD else -source.imm
                registers[name] = (kind, offset+delta)
    elif instruction.id == x86.X86_INS_XOR and len(operands) == 2:
        if operands[0].type == operands[1].type == capstone.CS_OP_REG and operands[0].reg == operands[1].reg:
            registers[full_register(instruction.reg_name(operands[0].reg))] = ('constant', 0)
    elif instruction.id == x86.X86_INS_PUSH:
        old = before.get('rsp')
        if old and old[0] == 'stack':
            registers['rsp'] = ('stack', old[1]-8)
            stack.pop(old[1]-8, None)
            result = value(instruction, operands[0], before, state[1])
            if result:
                stack[old[1]-8] = result
    elif instruction.id == x86.X86_INS_POP:
        old = before.get('rsp')
        if old and old[0] == 'stack' and operands[0].type == capstone.CS_OP_REG:
            name = instruction.reg_name(operands[0].reg)
            if old[1] in state[1]:
                registers[name] = state[1][old[1]]
            stack.pop(old[1], None)
            if name != 'rsp':
                registers['rsp'] = ('stack', old[1]+8)
    if instruction.id == x86.X86_INS_CALL:
        if 'rsp' in before:
            registers['rsp'] = before['rsp']
        for register in VOLATILE:
            registers.pop(register, None)
        # Unknown callees may write through an address into the caller's frame.
        if any(before.get(r, ('', 0))[0] == 'stack' for r in ARGUMENTS):
            stack.clear()
        else:
            pointer = before.get('rsp')
            if pointer and pointer[0] == 'stack':
                stack = {k:v for k,v in stack.items() if not pointer[1] <= k < pointer[1]+32}
    return registers, stack


def meet(previous, incoming):
    if previous is None:
        return dict(incoming[0]), dict(incoming[1])
    return tuple({k:v for k,v in left.items() if right.get(k) == v} for left, right in zip(previous, incoming))


def graph(instructions):
    by_address = {i.address:i for i in instructions}
    edges, unknown = {}, []
    for i in instructions:
        successors = []
        following = i.address+i.size
        if i.group(capstone.CS_GRP_RET):
            pass
        elif i.group(capstone.CS_GRP_JUMP):
            if i.operands and i.operands[0].type == capstone.CS_OP_IMM:
                target = i.operands[0].imm
                if target in by_address:
                    successors.append(target)
                else:
                    unknown.append([hex(i.address), 'jump_outside_owned_ranges', hex(target)])
            else:
                unknown.append([hex(i.address), 'indirect_jump'])
            if i.id != x86.X86_INS_JMP and following in by_address:
                successors.append(following)
        elif i.id in (x86.X86_INS_UD2, x86.X86_INS_INT3):
            unknown.append([hex(i.address), 'trap_terminator'])
        elif following in by_address:
            successors.append(following)
        else:
            unknown.append([hex(i.address), 'fallthrough_outside_owned_ranges'])
        edges[i.address] = successors
    return by_address, edges, unknown


def analyze_method(instructions, entry):
    by_address, edges, unknown = graph(instructions)
    if entry not in by_address:
        return [], [], dict(converged=False, reason='entry_not_decoded')
    states = {entry:({'rcx':('sprite', 0), 'rsp':('stack', 0)}, {})}
    pending = deque([entry]); queued = {entry}; iterations = 0
    while pending:
        pc = pending.popleft();queued.remove(pc);iterations += 1
        if iterations > 200000:
            return [], [], dict(converged=False, reason='iteration_budget')
        output = transfer(by_address[pc], states[pc])
        for following in edges[pc]:
            merged = meet(states.get(following), output)
            if merged != states.get(following):
                states[following] = merged
                if following not in queued:
                    pending.append(following);queued.add(following)
    accesses, escapes = [], []
    for pc, state in sorted(states.items()):
        i = by_address[pc];registers, stack = state
        for operand in i.operands:
            if operand.type != capstone.CS_OP_MEM:
                continue
            where = address(i, operand, registers)
            if where and where[0] == 'sprite':
                width = 1 if i.id == x86.X86_INS_LEA else max(operand.size, 1)
                if where[1] < QUAD[1] and where[1]+width > QUAD[0]:
                    accesses.append(dict(instruction=hex(pc), offset=hex(where[1]), width=operand.size,
                                         access='address_only' if i.id == x86.X86_INS_LEA else operand.access,
                                         text=i.mnemonic+' '+i.op_str))
        if i.id == x86.X86_INS_CALL or i.id == x86.X86_INS_JMP:
            args = {r:hex(registers[r][1]) for r in ARGUMENTS if r in registers and registers[r][0] == 'sprite'}
            sp = registers.get('rsp')
            if sp and sp[0] == 'stack':
                # These may be locals for shorter signatures; preserve the
                # uncertainty instead of silently missing GL's sixth argument.
                for slot in range(32, 96, 8):
                    pointer = stack.get(sp[1]+slot)
                    if pointer and pointer[0] == 'sprite':
                        args['potential_stack_arg'+str(slot//8+1)] = hex(pointer[1])
            if args:
                escapes.append(dict(instruction=hex(pc), kind='call_or_tail_arguments', aliases=args,
                                    text=i.mnemonic+' '+i.op_str))
        if i.group(capstone.CS_GRP_RET) and registers.get('rax', ('', 0))[0] == 'sprite':
            escapes.append(dict(instruction=hex(pc), kind='return_alias', offset=hex(registers['rax'][1])))
        if any(operand.type == capstone.CS_OP_MEM and operand.access & capstone.CS_AC_WRITE for operand in i.operands):
            if len(i.operands) > 1 and i.operands[1].type == capstone.CS_OP_REG:
                source = registers.get(i.reg_name(i.operands[1].reg))
                if source and source[0] == 'sprite':
                    dest = address(i, i.operands[0], registers) if i.operands[0].type == capstone.CS_OP_MEM else None
                    if not dest or dest[0] != 'stack':
                        escapes.append(dict(instruction=hex(pc), kind='stored_alias', offset=hex(source[1]),text=i.mnemonic+' '+i.op_str))
    return accesses, escapes, dict(converged=True, iterations=iterations, reachable=len(states),
                                  decoded=len(instructions), unknown_control_flow=unknown)


def sprite_instance_export(name):
    # x64 MSVC instance methods keep RCX=this, including const/non-void returns.
    marker='@CCSprite@cocos2d@@'
    return marker in name and bool(re.match(r'[A-Z]E[ABCD]A.',name.split(marker,1)[1]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    below_normal_priority()
    if args.image.name.lower() != 'libcocos2d.dll' or args.image.stat().st_size > 8*1024*1024:
        raise ValueError('Owned bounded Cocos image required')
    raw = args.image.read_bytes();pe = pefile.PE(data=raw, fast_load=True)
    try:
        if pe.FILE_HEADER.Machine != 0x8664 or (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage) != IDENTITIES['libcocos2d.dll']:
            raise ValueError('Unreviewed Cocos identity')
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION'], pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
        image = pe.get_memory_mapped_image();ranges = function_ranges(pe)
        decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);decoder.detail = True
        methods, skipped = [], []
        symbols = [s for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name and not s.forwarder]
        boundaries = sorted({s.address for s in symbols} | {a for a,_,_ in ranges})
        executable = [(s.VirtualAddress,s.VirtualAddress+s.Misc_VirtualSize)
                      for s in pe.sections if s.Characteristics & 0x20000000]
        for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if not symbol.name or b'@CCSprite@cocos2d@@' not in symbol.name:
                continue
            name = symbol.name.decode('ascii',errors='replace')
            get_quad = name == '?getQuad@CCSprite@cocos2d@@QEAA?AU_ccV3F_C4B_T2F_Quad@2@XZ'
            if not get_quad and not sprite_instance_export(name):
                skipped.append(name);continue
            owned = [(a,b) for a,b,owner in ranges if owner == symbol.address]
            boundary_kind = 'unwind_owned_ranges'
            if not owned:
                section_end = next((b for a,b in executable if a <= symbol.address < b),0)
                end = min([symbol.address+4096,section_end,len(image)] + [b for b in boundaries if b > symbol.address])
                if not symbol.address < end:
                    skipped.append(name);continue
                owned = [(symbol.address,end)]
                boundary_kind = 'bounded_export_slot_not_unwind_end'
            if get_quad:
                body = image[symbol.address:symbol.address+69]
                if hashlib.sha256(body).hexdigest() != 'ed1b5fe8cb05e6476344045298cdceb78196acee4880ec83181f5fb0dc213403':
                    skipped.append(name);continue
                owned = [(symbol.address,symbol.address+69)]
                boundary_kind = 'exact_reviewed_getQuad_body_RCX_source_RDX_output'
            instructions = [];complete = True;digest = hashlib.sha256()
            for begin,end in owned:
                block = list(decoder.disasm(image[begin:end],begin));instructions.extend(block)
                complete &= sum(i.size for i in block) == end-begin;digest.update(image[begin:end])
            accesses,escapes,status = analyze_method(instructions,symbol.address)
            methods.append(dict(name=name,entry=hex(symbol.address),body_sha256=digest.hexdigest(),boundary_kind=boundary_kind,
                                full_ranges_decoded=complete,status=status,accesses=accesses,escapes=escapes))
        result = dict(static_analysis_only=True,target_code_executed=False,all_cpu_consumers_proven=False,
                      module_sha256=hashlib.sha256(raw).hexdigest(),
                      entry_assumption='RCX is this for selected MSVC x64 instance exports (including const/non-void), plus the exact reviewed getQuad body',
                      limitations=['Unknown memory aliases, callees, inline callers, other modules and unseeded methods remain open',
                                   'This-relative address derivation is not a proof that native work may be suppressed'],
                      methods=methods,unseeded_exports=skipped)
        with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2)
        print(json.dumps(dict(methods=len(methods),with_quad_accesses=sum(bool(m['accesses']) for m in methods),
                              escapes=sum(len(m['escapes']) for m in methods),unseeded=len(skipped),
                              all_cpu_consumers_proven=False)))
    finally:
        pe.close()


if __name__ == '__main__':main()
