"""Bounded, offline R69 decoding. Selected-field access sites, never global closure or FPS."""
import argparse
from bisect import bisect_right
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
HEADER, ACTOR, ROW = struct.Struct('<8s9I4xQQd'), struct.Struct('<QII'), struct.Struct('<QQQII')


def decode(raw):
    if not HEADER.size <= len(raw) <= HEADER.size + 2 * ACTOR.size + 32768 * ROW.size:
        raise ValueError('Watch file outside capture budget')
    magic, version, row_size, actor_size, reasons, frames, count, objects, thread, features, game, cocos, percent = HEADER.unpack_from(raw)
    if (magic != b'CHLRD69\0' or version != 69 or row_size != ROW.size or actor_size != ACTOR.size or reasons or
            frames != 2 or not 0 < count <= 32768 or objects != 2 or not thread or features != 1 or
            not 0 <= percent <= 100 or not game or not cocos or game == cocos or (game | cocos) & 0xffff):
        raise ValueError('Invalid or incomplete selected-field capture')
    if len(raw) != HEADER.size + objects * ACTOR.size + count * ROW.size:
        raise ValueError('Watch file length differs from bounded header')
    actors = [ACTOR.unpack_from(raw, HEADER.size + i * ACTOR.size) for i in range(objects)]
    if any(not address or address & 7 or not uid for address, uid, _ in actors) or len({a[0] for a in actors}) != objects:
        raise ValueError('Invalid or duplicate actor identity')
    rows = [ROW.unpack_from(raw, HEADER.size + objects * ACTOR.size + i * ROW.size) for i in range(count)]
    if any(not rip or not exception or not 0 < mask < 16 or not frame for rip, exception, ticks, mask, frame in rows):
        raise ValueError('Invalid watch row')
    tags = sorted({row[4] for row in rows})
    if len(tags) != 2 or tags[1] != tags[0] + 1 or any(a[4] > b[4] for a, b in zip(rows, rows[1:])):
        raise ValueError('Both consecutive frames must contain observed field accesses')
    return dict(game_base=game, cocos_base=cocos, percent=percent, thread=thread,
                actors=[dict(address=hex(a), uid=u, object_id=o) for a, u, o in actors], frames=tags), rows


def load_image(path, digest):
    sys.path[:0] = [str(ROOT / 'build-research/python'), str(ROOT / 'research/R4-29')]
    import pefile
    from pe_ranges import function_ranges
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != digest.lower():
        raise ValueError('Native image differs from pinned capture dependency')
    pe = pefile.PE(data=raw)
    try:
        if pe.FILE_HEADER.Machine != 0x8664:
            raise ValueError('Expected x64 native image')
        spans = function_ranges(pe)
        exports = {e.address: e.name.decode('ascii', errors='replace') for e in getattr(getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', []) if e.name and not e.forwarder}
        return dict(image=pe.get_memory_mapped_image(), spans=spans, starts=[s[0] for s in spans], exports=exports)
    finally:
        pe.close()


def access_site(image, rip):
    import capstone
    spans = image['spans']; index = bisect_right(image['starts'], rip - 1) - 1
    if index < 0 or not spans[index][0] < rip <= spans[index][1]:
        return dict(resolution='no_unwind_boundary', post_rva=hex(rip))
    begin, end, owner = spans[index]
    result = dict(resolution='unresolved_instruction', post_rva=hex(rip), unwind_owner=hex(owner), symbol=image['exports'].get(owner, ''))
    if rip - begin > 1024 * 1024:
        return result
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for instruction in decoder.disasm(image['image'][begin:rip], begin):
        if instruction.address + instruction.size == rip:
            result.update(instruction_rva=hex(instruction.address), instruction=instruction.mnemonic + ' ' + instruction.op_str,
                          resolution='linear_unwind_boundary' if not instruction.mnemonic.startswith(('rep', 'loop')) else 'repeated_instruction_ambiguous')
    return result


def analyze(path, provenance, game, cocos):
    if provenance.get('native_field_watch_diagnostic') is not True or provenance.get('native') is not False or provenance.get('mod_profile') != 'isolated':
        raise ValueError('Expected isolated Chlorium-active R69 diagnostic provenance')
    meta, rows = decode(path.read_bytes())
    native = [('game', meta['game_base'], load_image(game, provenance['game_sha256'])),
              ('cocos', meta['cocos_base'], load_image(cocos, 'eb43e5e6d6e2ce4d3109dcb11fbb6634b16fb7204da1532bbf62a359dc98b94c'))]
    counts = Counter((rip, exception, mask, frame) for rip, exception, ticks, mask, frame in rows)
    sites = []
    for (rip, exception, mask, frame), count in counts.most_common():
        item = dict(post_address=hex(rip), exception_address=hex(exception), mask=mask, frame=frame, hits=count)
        item['fields'] = [f'actor{i // 2}.position_{"xy"[i % 2]}' for i in range(4) if mask & 1 << i]
        for name, base, image in native:
            if base <= rip < base + len(image['image']):
                item.update(module=name, **access_site(image, rip - base)); break
        else:
            item.update(module='unresolved', resolution='outside_pinned_native_images')
        sites.append(item)
    return dict(diagnostic_only=True, global_reader_closure=False, performance_evidence=False,
                scope='Two selected decoration actors; first frame partial, second complete; main thread only. Post-access hardware traps; linear unwind decoding is not a control-flow proof.',
                trace_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), provenance=provenance, capture=meta, rows=len(rows), sites=sites)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path); parser.add_argument('provenance', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT / 'tools'))
    from audit_json import read_events
    report = Path(str(args.provenance).removesuffix('.provenance.json'))
    events = read_events(report)
    witnesses = [e for e in events if e.get('event') == 'native_field_watch_r69']
    if (not events or events[-1].get('event') != 'finished' or events[-1].get('success') is not True or
            len(witnesses) != 1 or not witnesses[0].get('read') or not witnesses[0].get('complete') or
            witnesses[0].get('reasons') or witnesses[0].get('failure')):
        raise ValueError('Independent auditor did not certify a completed diagnostic')
    result = analyze(args.trace, json.loads(args.provenance.read_text(encoding='utf-8-sig')),
                     Path('C:/Users/stoti/Chlorium/test-game/GeometryDash.exe'), Path('C:/Users/stoti/Chlorium/test-game/libcocos2d.dll'))
    if witnesses[0].get('rows') != result['rows'] or witnesses[0].get('objects') != 2 or witnesses[0].get('frames') != 2:
        raise ValueError('Trace differs from independent auditor counters')
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2); output.write('\n')
    print(json.dumps(dict(rows=result['rows'], sites=len(result['sites']), global_reader_closure=False)))
