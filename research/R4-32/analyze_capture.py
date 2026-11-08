"""Bounded native CPU-pass evidence. No process access or FPS verdict."""
import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct

FILE = struct.Struct('<8s4I')
FRAME = struct.Struct('<QQ4IQQ')
DRAW = struct.Struct('<4Q16I')
SPRITE_V1 = struct.Struct('<2Q22I')
SPRITE = struct.Struct('<2Q26IQ16I')
QUAD_BYTES = 96
MAX_QUADS, MAX_DRAWS, MAX_BYTES = 65536, 2048, 64 * 1024 * 1024


@dataclass
class FrameView:
    number: int
    percent: float
    reasons: int
    context: int
    scene: int
    draws: list
    quads: memoryview
    sprites: memoryview
    version: int
    sprite_layout: struct.Struct


def decode(raw):
    if len(raw) > MAX_BYTES:
        raise ValueError('Capture exceeds the file budget')
    data = memoryview(raw)
    cursor = 0

    def take(size):
        nonlocal cursor
        if size > len(data) - cursor:
            raise ValueError('Truncated native capture')
        block = data[cursor:cursor + size]
        cursor += size
        return block

    magic, count, draw_size, sprite_size, quad_size = FILE.unpack(take(FILE.size))
    version = {b'CHLPASS1': 1, b'CHLPASS2': 2, b'CHLPASS3': 3}.get(magic)
    sprite_layout = SPRITE_V1 if version == 1 else SPRITE
    if not version or count not in (1, 2) or (draw_size, sprite_size, quad_size) != (DRAW.size, sprite_layout.size, QUAD_BYTES):
        raise ValueError('Unknown capture layout')
    frames = []
    for _ in range(count):
        number, percent_bits, draws, quads, sprites, reasons, context, scene = FRAME.unpack(take(FRAME.size))
        percent = struct.unpack('<d', struct.pack('<Q', percent_bits))[0]
        if (draws > MAX_DRAWS or quads > MAX_QUADS or sprites != quads or
                reasons & ~511 or not math.isfinite(percent) or not 0 <= percent <= 100):
            raise ValueError('Invalid native frame bounds')
        records = list(DRAW.iter_unpack(take(draws * DRAW.size)))
        quad_data, sprite_data = take(quads * QUAD_BYTES), take(sprites * sprite_layout.size)
        expected_offset = 0
        for ordinal, record in enumerate(records):
            (_, _, _, _, _, _, _, scope, actual_ordinal, first, amount,
             quad_offset, sprite_offset, mapped, why, _, _, total, capacity, fp_state) = record
            if (actual_ordinal != ordinal or quad_offset != expected_offset or sprite_offset != expected_offset or
                    first > total or total > capacity or amount > total - first or amount > quads - expected_offset or
                    mapped > amount or why & ~60 or (fp_state != 0 if version == 1 else fp_state & ~0xffff)):
                raise ValueError('Invalid native draw range or order')
            actual_mapped = 0
            for index in range(amount):
                sprite = sprite_layout.unpack_from(sprite_data, (sprite_offset + index) * sprite_layout.size)
                if sprite[0] and sprite[3] == first + index:
                    actual_mapped += 1
                if sprite[5] not in (0, 1) or (sprite[-1] if version == 1 else False) or sprite[4] & ~(511 if version == 1 else 2047):
                    raise ValueError('Invalid sprite flags')
            if version == 2:
                for index in range(amount):
                    item = sprite_layout.unpack_from(sprite_data, (sprite_offset + index) * sprite_layout.size)
                    known = bool(item[30] & 0x80000000)
                    if (item[34] or item[30] & ~0x80007fff or bool(item[28]) != known or
                            (not known and any(item[28:45]))):
                        raise ValueError('Invalid root metadata')
                    if not item[30] & (1 << 14) and item[31] > 10:
                        raise ValueError('Unbounded root group membership')
            if mapped != actual_mapped:
                raise ValueError('Sprite ownership count differs from the record')
            expected_offset += amount
        if expected_offset != quads:
            raise ValueError('Unaccounted native quad data')
        frames.append(FrameView(number, percent, reasons, context, scene, records, quad_data, sprite_data, version, sprite_layout))
    if cursor != len(data):
        raise ValueError('Trailing native capture bytes')
    return frames


def sprite_at(frame, index):
    values = frame.sprite_layout.unpack_from(frame.sprites, index * frame.sprite_layout.size)
    return values + (0,) * (45 - len(values)) if frame.version == 1 else values


def draw_key(draw):
    return draw[:8] + draw[9:11] + draw[15:19]


def owner_resources(frame):
    resources, ambiguous, order, scopes, occurrences = {}, set(), [], {}, {}
    for draw in frame.draws:
        if draw[14]:
            continue
        base = (draw[0], draw[4], draw[1], draw[5], draw[2], draw[6], draw[3], draw[15], draw[16])
        scope = (base, draw[7])
        if scope not in scopes:
            scopes[scope] = occurrences.get(base, 0)
            occurrences[base] = scopes[scope] + 1
        for index in range(draw[10]):
            sprite = sprite_at(frame, draw[12] + index)
            if not sprite[0] or not sprite[5]:
                continue
            key = base + (scopes[scope], sprite[0], sprite[2])
            if key in resources or key in ambiguous:
                resources.pop(key, None)
                ambiguous.add(key)
                continue
            quad_at = (draw[11] + index) * QUAD_BYTES
            resources[key] = (frame.quads[quad_at:quad_at + QUAD_BYTES], sprite)
            order.append(key)
    return resources, ambiguous, order


def compare_resources(a, b):
    left, la, lo = owner_resources(a)
    right, ra, ro = owner_resources(b)
    common = left.keys() & right.keys()
    result = dict(retained_owner_resources=len(common), added_owner_resources=len(right.keys() - left.keys()),
                  retired_owner_resources=len(left.keys() - right.keys()), ambiguous_owner_keys=[len(la), len(ra)],
                  retained_owner_order_changed=[k for k in lo if k in common] != [k for k in ro if k in common],
                  retained_quad_identical=0, retained_matrix_changed=0, retained_local_geometry_changed=0,
                  retained_color_changed=0, retained_uv_changed=0, affine_hint_count=0, root_known_resources=0,
                  decoration_root_resources=0, area_parent_resources=0, group_parent_resources=0,
                  root_membership_changed=0, real_opacity_zero=[0,0])
    for key in common:
        qa, sa = left[key]
        qb, sb = right[key]
        uv_changed = any(qa[v+16:v+24] != qb[v+16:v+24] for v in (0,24,48,72))
        result['retained_quad_identical'] += qa == qb
        result['retained_matrix_changed'] += sa[6:12] != sb[6:12]
        result['retained_local_geometry_changed'] += (sa[12:18] != sb[12:18] or sa[19:23] != sb[19:23] or
                                                     bool((sa[4] ^ sb[4]) & 120) or uv_changed)
        result['retained_color_changed'] += any(qa[v+12:v+16] != qb[v+12:v+16] for v in (0,24,48,72))
        result['retained_uv_changed'] += uv_changed
        result['affine_hint_count'] += not ((sa[4] | sb[4]) & 193)
        if a.version >= 3 and b.version >= 3:
            result['root_known_resources'] += bool(sa[30] & sb[30] & 0x80000000)
            result['decoration_root_resources'] += bool(sa[30] & sb[30] & 1)
            result['area_parent_resources'] += bool((sa[30] | sb[30]) & 4)
            result['group_parent_resources'] += bool((sa[30] | sb[30]) & 8)
            result['root_membership_changed'] += sa[28:32] != sb[28:32] or sa[35:45] != sb[35:45]
            result['real_opacity_zero'][0] += not bool(sa[4] & 512)
            result['real_opacity_zero'][1] += not bool(sb[4] & 512)
    return result


def summarize(raw):
    frames = decode(raw)
    result = dict(diagnostic_only=True, replacement_ready=False, fps_gain_proven=False,
                  capture_sha256=hashlib.sha256(raw).hexdigest(), frames=len(frames),
                  capture_versions=[f.version for f in frames], branch_inputs_complete=all(f.version >= 3 for f in frames),
                  frame_numbers=[f.number for f in frames], percentages=[f.percent for f in frames],
                  frame_reasons=[f.reasons for f in frames], draw_counts=[len(f.draws) for f in frames],
                  quad_counts=[len(f.quads) // QUAD_BYTES for f in frames],
                  mapped_quads=[sum(d[13] for d in f.draws) for f in frames],
                  unknown_draws=[sum(bool(d[14]) for d in f.draws) for f in frames],
                  limitation='CPU atlas observations only; no GL-state, native-callback suppression or pixel equivalence proof.')
    comparable = (len(frames) == 2 and frames[1].number == frames[0].number + 1 and
                  all(not f.reasons and f.context and f.scene and f.draws for f in frames) and
                  frames[0].context == frames[1].context and frames[0].scene == frames[1].scene)
    result['two_frame_context_valid'] = bool(comparable)
    if not comparable:
        return result
    a, b = frames
    counters = dict(matched_draws=0, compared_quads=0, identical_quads=0, position_changed=0,
                    color_changed=0, uv_changed=0, same_mapped_owner=0, matrix_changed=0,
                    local_rect_changed=0, anchor_changed=0, topology_mismatched_calls=abs(len(a.draws) - len(b.draws)))
    for left, right in zip(a.draws, b.draws):
        if draw_key(left) != draw_key(right):
            counters['topology_mismatched_calls'] += 1
            continue
        counters['matched_draws'] += 1
        for i in range(left[10]):
            ia, ib = left[11] + i, right[11] + i
            qa, qb = a.quads[ia * 96:(ia + 1) * 96], b.quads[ib * 96:(ib + 1) * 96]
            counters['compared_quads'] += 1
            counters['identical_quads'] += qa == qb
            for key, offset, size in (('position_changed', 0, 12), ('color_changed', 12, 4), ('uv_changed', 16, 8)):
                counters[key] += any(qa[v + offset:v + offset + size] != qb[v + offset:v + offset + size] for v in (0, 24, 48, 72))
            sa = sprite_at(a, left[12] + i)
            sb = sprite_at(b, right[12] + i)
            same_owner = bool(sa[0] and sa[:4] == sb[:4] and sa[5] and sb[5] and not left[14] and not right[14])
            counters['same_mapped_owner'] += same_owner
            if same_owner:
                counters['matrix_changed'] += sa[6:12] != sb[6:12]
                counters['local_rect_changed'] += sa[12:16] != sb[12:16]
                counters['anchor_changed'] += sa[16:18] != sb[16:18]
    result.update(counters)
    result['owner_resource_alignment'] = compare_resources(a, b)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.capture.stat().st_size > MAX_BYTES:
        raise ValueError('Capture exceeds the file budget')
    provenance_path = Path(str(args.capture).removesuffix('.pass-capture.bin') + '.provenance.json')
    provenance = json.loads(provenance_path.read_text(encoding='utf-8-sig'))
    if not provenance.get('native_pass_capture_diagnostic') or provenance.get('native') is not True:
        raise ValueError('Expected isolated native pass-capture provenance')
    result = summarize(args.capture.read_bytes())
    result['provenance'] = {k: provenance.get(k) for k in ('level', 'game_sha256', 'measurement_sha256', 'level_sha256', 'target_fps')}
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps(result))
