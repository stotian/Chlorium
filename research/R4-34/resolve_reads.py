"""Resolve bounded data-watch events; never infer absence of other CPU readers."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'build-research/python'), str(ROOT / 'research/R4-29')]
import capstone
import pefile
from pe_ranges import function_ranges


def load_observation(path):
    if path.stat().st_size > 2 * 1024 * 1024:
        raise ValueError('Read observation budget')
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('diagnostic_only') is not True or data.get('fps_evidence') is not False:
        raise ValueError('Expected a separate diagnostic, never FPS evidence')
    events = data.get('events')
    if not isinstance(events, list) or len(events) > 512:
        raise ValueError('Bounded event list required')
    for row in events:
        if not isinstance(row.get('slots'), int) or not 1 <= row['slots'] <= 15:
            raise ValueError('Invalid hardware slot mask')
        text = row.get('preceding_bytes_at_finish')
        if text is not None and (not isinstance(text, str) or len(text) != 30):
            raise ValueError('Expected exactly fifteen live instruction bytes')
        if text is not None:
            bytes.fromhex(text)
    return data


def resolve_instruction(image, ranges, after, live, decoder, cache):
    owners = [(a, b, owner) for a, b, owner in ranges if a < after <= b]
    if len(owners) != 1:
        return dict(status='unbounded_procedure')
    a, b, owner = owners[0]
    if (a, b) not in cache:
        instructions = list(decoder.disasm(image[a:b], a))
        cache[a, b] = {i.address + i.size: i for i in instructions} if sum(i.size for i in instructions) == b-a else {}
    instruction = cache[a, b].get(after)
    if instruction is None:
        return dict(status='not_instruction_end')
    if not live.endswith(bytes(instruction.bytes)):
        return dict(status='runtime_instruction_differs')
    accesses = [op.access for op in instruction.operands if op.type == capstone.CS_OP_MEM]
    if len(accesses) != 1 or accesses[0] not in (capstone.CS_AC_READ, capstone.CS_AC_WRITE):
        access = 'ambiguous'
    else:
        access = 'read' if accesses[0] == capstone.CS_AC_READ else 'write'
    return dict(status='instruction_verified_at_finish', access=access, owner_rva=hex(owner),
                instruction_rva=hex(instruction.address), instruction=instruction.mnemonic + ' ' + instruction.op_str)


def exported_leaf(image, start, end, decoder):
    if not 0 < end-start <= 256:
        return None
    for instruction in decoder.disasm(image[start:end], start):
        if instruction.group(capstone.CS_GRP_RET):
            return start, instruction.address+instruction.size, start
        if any(instruction.group(g) for g in (capstone.CS_GRP_CALL, capstone.CS_GRP_JUMP, capstone.CS_GRP_INT)):
            return None
    return None


def analyze(path, auditor):
    data = load_observation(path)
    prefix = str(path).removesuffix('.quad-reads.json')
    provenance = json.loads(Path(prefix + '.provenance.json').read_text(encoding='utf-8-sig'))
    if not provenance.get('native_quad_reads_diagnostic') or not provenance.get('native_pass_capture_diagnostic') or not provenance.get('native'):
        raise ValueError('Explicit isolated native read-watch provenance required')
    package = auditor.read_bytes()
    if hashlib.sha256(package).hexdigest() != provenance['measurement_sha256'].lower():
        raise ValueError('Auditor package differs from the observed run')
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    decoder.detail = True
    modules, hashes, output = {}, {}, []
    for row in data['events']:
        name = row.get('module', 'unknown')
        live = row.get('preceding_bytes_at_finish')
        result = dict(module=name, slots=row['slots'], rva_after=row.get('rva_after'))
        if live is None:
            result['status'] = 'missing_runtime_instruction_bytes'
        else:
            if name not in modules:
                if name.lower() == 'local.chlorium.audit.dll':
                    with zipfile.ZipFile(auditor) as archive:
                        raw = archive.read('local.chlorium.audit.dll')
                else:
                    source = Path(row.get('module_file', ''))
                    if not source.is_file() or source.name.lower() != name.lower() or source.stat().st_size > 64*1024*1024:
                        modules[name] = None
                        output.append(dict(result, status='module_file_unavailable'))
                        continue
                    raw = source.read_bytes()
                digest = hashlib.sha256(raw).hexdigest()
                if name.lower() == 'geometrydash.exe' and digest != provenance['game_sha256'].lower():
                    raise ValueError('Game image changed')
                pe = pefile.PE(data=raw, fast_load=True)
                pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION'], pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
                image, ranges = pe.get_memory_mapped_image(), function_ranges(pe)
                if name.lower() == 'libcocos2d.dll':
                    symbols = getattr(getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', [])
                    for symbol in symbols:
                        if symbol.name and symbol.name.startswith(b'?updateQuad@CCTextureAtlas@'):
                            next_symbol = min((s.address for s in symbols if s.address > symbol.address), default=0)
                            leaf = exported_leaf(image, symbol.address, next_symbol, decoder)
                            if leaf and not any(a < leaf[1] and leaf[0] < b for a, b, _ in ranges):
                                ranges.append(leaf)
                modules[name] = (image, ranges, {})
                hashes[name] = digest
                pe.close()
            if modules[name] is None:
                result['status'] = 'module_file_unavailable'
            else:
                image, ranges, cache = modules[name]
                result.update(resolve_instruction(image, ranges, row['rva_after'], bytes.fromhex(live), decoder, cache))
        result['raw_stack_module_candidates_not_unwound'] = row.get('raw_stack_module_candidates_not_unwound', [])
        output.append(result)
    counts = Counter((r.get('module'), r.get('access', 'unknown'), r['status']) for r in output)
    return dict(diagnostic_only=True, fps_gain_proven=False, all_cpu_consumers_proven=False,
                observation_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                level=provenance['level'], observation_complete=data.get('observation_complete', False),
                byte_verification_scope='Instruction bytes read at finish; not an atomic code snapshot at the event',
                scope=data.get('scope'), module_sha256=hashes,
                counts=[dict(module=k[0], access=k[1], status=k[2], events=v) for k, v in sorted(counts.items())], events=output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('observation', type=Path)
    parser.add_argument('auditor_package', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = analyze(args.observation, args.auditor_package)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({key: result[key] for key in ('level', 'observation_complete', 'counts', 'all_cpu_consumers_proven')}))
