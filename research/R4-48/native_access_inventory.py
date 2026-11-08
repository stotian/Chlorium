"""Static potential quad/pose accesses in owned PE files; never execute target code."""
import argparse
from collections import Counter, defaultdict
import csv
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'build-research/python'), str(ROOT / 'research/R4-29'),
                str(ROOT / 'research/R4-34')]
import capstone
from capstone import x86_const as x86
import pefile
from pe_ranges import function_ranges
from resolve_reads import exported_leaf

FIELDS = {'sprite_quad': (0x1cc, 0x22c), 'batch_transform': (0x17c, 0x194),
          'sprite_rectangle': (0x1a8, 0x1b8), 'sprite_offset': (0x1bc, 0x1c4)}
IDENTITIES = {'geometrydash.exe': (0x69702da0, 0xa3a000),
              'libcocos2d.dll': (0x69702d99, 0x1c5000)}


def below_normal_priority():
    if os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        kernel.SetPriorityClass.argtypes = (ctypes.c_void_p, ctypes.c_uint32)
        if not kernel.SetPriorityClass(kernel.GetCurrentProcess(), 0x4000):
            raise OSError('Unable to set analysis process below normal priority')


def potential_fields(instruction):
    """Displacement overlap is a candidate, not a proven CCSprite alias."""
    for operand in instruction.operands:
        if operand.type != capstone.CS_OP_MEM or operand.mem.base in (0, x86.X86_REG_RIP):
            continue
        for field, (begin, end) in FIELDS.items():
            address_only = instruction.id == x86.X86_INS_LEA
            width = 1 if address_only else max(operand.size, 1)
            if operand.mem.disp < end and operand.mem.disp + width > begin:
                if address_only:
                    access = 'address_only_not_memory_read'
                elif operand.access == capstone.CS_AC_READ:
                    access = 'read'
                elif operand.access == capstone.CS_AC_WRITE:
                    access = 'write'
                elif operand.access == capstone.CS_AC_READ | capstone.CS_AC_WRITE:
                    access = 'read_write'
                else:
                    access = 'unknown'
                yield dict(field=field, access=access, displacement=hex(operand.mem.disp),
                           width=operand.size, base=instruction.reg_name(operand.mem.base),
                           index=instruction.reg_name(operand.mem.index), scale=operand.mem.scale,
                           stack_base=operand.mem.base == x86.X86_REG_RSP)


def inspect(path):
    if path.name.lower() not in IDENTITIES or not path.is_file() or path.stat().st_size > 64*1024*1024:
        raise ValueError('Expected bounded owned GD/Cocos PE file')
    raw = path.read_bytes()
    pe = pefile.PE(data=raw, fast_load=True)
    try:
        expected = IDENTITIES[path.name.lower()]
        if (pe.FILE_HEADER.Machine != 0x8664 or
            (pe.FILE_HEADER.TimeDateStamp, pe.OPTIONAL_HEADER.SizeOfImage) != expected):
            raise ValueError('Unreviewed PE identity')
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY[name] for name in (
            'IMAGE_DIRECTORY_ENTRY_EXCEPTION', 'IMAGE_DIRECTORY_ENTRY_EXPORT', 'IMAGE_DIRECTORY_ENTRY_IMPORT')])
        image = pe.get_memory_mapped_image()
        if len(image) > 64*1024*1024:
            raise ValueError('Mapped image budget')
        decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        decoder.detail = True
        decoder.skipdata = False
        ranges = function_ranges(pe)
        if len(ranges) > 100000:
            raise ValueError('Procedure budget')
        exports = defaultdict(list)
        symbols = getattr(getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', [])
        for symbol in symbols:
            if symbol.name and not symbol.forwarder:
                exports[symbol.address].append(symbol.name.decode('ascii', errors='replace'))
        imports = {}
        for module in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
            for symbol in module.imports:
                imports[symbol.address - pe.OPTIONAL_HEADER.ImageBase] = (
                    module.dll.decode('ascii', errors='replace') + '!' +
                    (symbol.name.decode('ascii', errors='replace') if symbol.name else '#'+str(symbol.ordinal)))
        executable = [(s.VirtualAddress, s.VirtualAddress+min(s.Misc_VirtualSize, len(image)-s.VirtualAddress))
                      for s in pe.sections if s.Characteristics & 0x20000000 and s.VirtualAddress < len(image)]
        # Existing resolver admits only bounded, straight-line exported leaves.
        for start in sorted(exports):
            if any(a <= start < b for a, b, _ in ranges) or not any(a <= start < b for a, b in executable):
                continue
            end = min([start+256, len(image)] + [rva for rva in exports if rva > start] +
                      [a for a, _, _ in ranges if a > start])
            leaf = exported_leaf(image, start, end, decoder)
            if leaf and not any(a < leaf[1] and leaf[0] < b for a, b, _ in ranges):
                ranges.append(leaf)
        ranges.sort()
        candidates, calls, incomplete = [], [], []
        total_instructions = decoded_bytes = 0
        owners = defaultdict(lambda: dict(ranges=[], names=[], direct_calls=set()))
        for begin, end, owner in ranges:
            if end-begin > 1024*1024:
                raise ValueError('Single procedure budget')
            record = owners[owner]
            record['ranges'].append([hex(begin), hex(end)])
            record['names'] = exports.get(owner, [])
            cursor = begin
            for instruction in decoder.disasm(image[begin:end], begin):
                if instruction.address != cursor:
                    break
                cursor += instruction.size
                total_instructions += 1
                if total_instructions > 5000000:
                    raise ValueError('Instruction budget')
                context = dict(owner=hex(owner), owner_exports=' | '.join(record['names']),
                               instruction=hex(instruction.address),
                               text=instruction.mnemonic+' '+instruction.op_str,
                               bytes=bytes(instruction.bytes).hex())
                for field in potential_fields(instruction):
                    candidates.append(dict(context, **field))
                if instruction.id in (x86.X86_INS_CALL, x86.X86_INS_JMP) and instruction.operands:
                    op = instruction.operands[0]
                    target, named = None, ''
                    if op.type == capstone.CS_OP_IMM:
                        target = op.imm
                        named = ' | '.join(exports.get(target, []))
                        if instruction.id == x86.X86_INS_CALL:
                            record['direct_calls'].add(target)
                    elif op.type == capstone.CS_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                        named = imports.get(instruction.address+instruction.size+op.mem.disp, '')
                    if named or target in exports:
                        calls.append(dict(context, target=hex(target) if target is not None else '', target_name=named))
            decoded_bytes += cursor-begin
            if cursor != end:
                incomplete.append(dict(owner=hex(owner), begin=hex(begin), undecoded_begin=hex(cursor), end=hex(end)))
        counts = Counter((r['field'], r['access']) for r in candidates)
        return dict(module=path.name, file=str(path.resolve()), sha256=hashlib.sha256(raw).hexdigest(),
                    timestamp=hex(pe.FILE_HEADER.TimeDateStamp), image_bytes=len(image),
                    ranges=len(ranges), decoded_bytes=decoded_bytes, instructions=total_instructions,
                    incomplete_ranges=incomplete, exports_with_code=len(exports),
                    counts=[dict(field=f, access=a, count=n) for (f, a), n in sorted(counts.items())],
                    candidates=candidates, named_calls=calls,
                    owners={hex(k): dict(v, direct_calls=[hex(x) for x in sorted(v['direct_calls'])]) for k, v in owners.items()})
    finally:
        pe.close()


def write_csv(path, rows, columns):
    with path.open('x', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('images', nargs='+', type=Path)
    args = parser.parse_args()
    if not 1 <= len(args.images) <= 2 or len({p.name.lower() for p in args.images}) != len(args.images):
        raise ValueError('One distinct owned image per module')
    below_normal_priority()
    args.output.mkdir(parents=True, exist_ok=False)
    modules = [inspect(path) for path in args.images]
    for module in modules:
        name = module['module']
        write_csv(args.output/(name+'.accesses.csv'), module.pop('candidates'),
                  ['owner', 'owner_exports', 'instruction', 'text', 'bytes', 'field', 'access',
                   'displacement', 'width', 'base', 'index', 'scale', 'stack_base'])
        write_csv(args.output/(name+'.calls.csv'), module.pop('named_calls'),
                  ['owner', 'owner_exports', 'instruction', 'text', 'bytes', 'target', 'target_name'])
        with (args.output/(name+'.owners.local.json')).open('x', encoding='utf-8') as stream:
            json.dump(module.pop('owners'), stream)
    result = dict(static_analysis_only=True, target_code_executed=False, fps_evidence=False,
                  all_cpu_consumers_proven=False,
                  scope='Potential displacement-overlap accesses in unwind procedures and bounded exported leaves',
                  limitations=['No pointer-type proof; unrelated structures/stack locals are false positives',
                               'No negative/interior-pointer aliases, arbitrary copies or pointer escape closure',
                               'Indirect calls, dynamic code, uncovered code and other modules remain unproven'],
                  modules=modules)
    with (args.output/'inventory.local.json').open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({m['module']: {'instructions': m['instructions'], 'counts': m['counts'],
                                   'incomplete_ranges': len(m['incomplete_ranges'])} for m in modules}))


if __name__ == '__main__':
    main()
