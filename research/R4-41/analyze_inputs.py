"""Observed native input eligibility, never game performance or renderer approval."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "research/R4-32"))
from analyze_capture import decode
from PIL import Image

VERTEX = struct.Struct('<6I')
ONE = 0x3f800000


def texel_floor(bits, size):
    exponent = bits >> 23
    mantissa = (bits & 0x7fffff) | (0x800000 if exponent else 0)
    return (mantissa * size) >> (150 - exponent if exponent else 149)


def safe_depth(z, scale):
    z, scale = z & 0x7fffffff, scale & 0x7fffffff
    if z >= 0x7f800000 or scale >= 0x7f800000:
        return False
    if not z or not scale:
        return True
    ze, se = z >> 23, scale >> 23
    product = ((z & 0x7fffff) | (0x800000 if ze else 0)) * ((scale & 0x7fffff) | (0x800000 if se else 0))
    shift = 298 - (ze or 1) - (se or 1)
    return shift >= 48 or (shift >= 0 and product <= 1 << shift)


def matrix_supported(matrix):
    if not isinstance(matrix, list) or len(matrix) != 16 or any(type(x) is not int or not 0 <= x <= 0xffffffff for x in matrix):
        return False
    if any((x & 0x7fffffff) >= 0x7f800000 or (i != 10 and (x & 0x7fffffff) > 0x4f800000) for i, x in enumerate(matrix)):
        return False
    return matrix[15] == ONE and not any(matrix[i] & 0x7fffffff for i in (2, 6, 14, 3, 7, 11))


class Mask:
    def __init__(self, metadata, directory):
        w, h = metadata['width'], metadata['height']
        if type(w) is not int or type(h) is not int or not 0 < w <= 4096 or not 0 < h <= 4096:
            raise ValueError('Texture dimensions outside the snapshot contract')
        name = metadata['file']
        if not isinstance(name, str) or Path(name).name != name or '/' in name or '\\' in name:
            raise ValueError('Texture path must be a local basename')
        file = directory / name
        if file.resolve().parent != directory.resolve() or file.stat().st_size != w * h * 4 or file.stat().st_size > 32 * 1024 * 1024:
            raise ValueError('Texture bytes/path differ from the bounded receipt')
        raw = file.read_bytes()
        self.sha256 = hashlib.sha256(raw).hexdigest()
        self.alpha = Image.frombytes('RGBA', (w, h), raw).getchannel('A')
        self.width, self.height = w, h
        self.cache = {}

    def footprint(self, vertices, whole):
        if any(v[3] >> 24 != 255 for v in vertices):
            return False
        us, vs = [], []
        for v in vertices:
            for bits, out in ((v[4], us), (v[5], vs)):
                absolute = bits & 0x7fffffff
                if absolute > ONE or (bits >> 31 and absolute):
                    return False
                out.append(absolute)
        x0 = max(0, texel_floor(min(us), self.width) - 3)
        x1 = min(self.width - 1, texel_floor(max(us), self.width) + 3) + 1
        y0 = max(0, texel_floor(min(vs), self.height) - 3)
        y1 = min(self.height - 1, texel_floor(max(vs), self.height) + 3) + 1
        key = x0, y0, x1, y1
        if key not in self.cache:
            self.cache[key] = self.alpha.crop(key).getextrema()
        return self.cache[key] == (255, 255) if whole else self.cache[key][1] == 255

    def opaque(self, vertices):
        return self.footprint(vertices, True)

    def maybe_opaque(self, vertices):
        return self.footprint(vertices, False)


def state_reason(row):
    if row.get('active_texture') != 0x84c0 or row.get('texture_name') != row.get('expected_texture'):
        return 'texture_binding'
    if not row.get('program_linked') or row.get('program') != row.get('expected_program') or row.get('sampler_unit') != 0:
        return 'program_or_sampler'
    if row.get('samples') != 0 or row.get('depth_test') or row.get('stencil_test') or not row.get('blend'):
        return 'depth_stencil_samples_or_blend'
    if row.get('blend_src_rgb') not in (1, 0x302) or row.get('blend_src_alpha') not in (1, 0x302) or row.get('blend_dst_rgb') != 0x303 or row.get('blend_dst_alpha') != 0x303 or row.get('blend_equation_rgb') != 0x8006 or row.get('blend_equation_alpha') != 0x8006:
        return 'blend_factors'
    if row.get('min_filter') not in (0x2600, 0x2601) or row.get('mag_filter') not in (0x2600, 0x2601) or row.get('wrap_s') != 0x812f or row.get('wrap_t') != 0x812f or row.get('base_level') != 0 or row.get('alpha_swizzle') != 0x1906 or row.get('sampler_binding') != 0 or row.get('anisotropy_bits') != ONE:
        return 'sampler_state'
    if not matrix_supported(row.get('mvp_bits')):
        return 'mvp'
    return None


def analyze(base):
    provenance = json.loads(Path(str(base) + '.provenance.json').read_text(encoding='utf-8-sig'))
    if not provenance.get('native') or not provenance.get('native_pass_capture_diagnostic') or not provenance.get('opaque_input_census_diagnostic'):
        raise ValueError('Expected the separate isolated native opaque-input diagnostic')
    raw = Path(str(base) + '.pass-capture.bin').read_bytes()
    frames = decode(raw)
    if len(frames) != 2 or frames[1].number != frames[0].number + 1 or any(f.reasons for f in frames):
        raise ValueError('Incomplete native capture interval')
    census = json.loads(Path(str(base) + '.opaque-inputs.json').read_text(encoding='utf-8'))
    if census.get('failed') or not census.get('diagnostic_only') or census.get('fps_evidence') is not False:
        raise ValueError('Failed/mislabelled input census')
    textures = {v['index']: v for v in census['textures']}
    if len(textures) != len(census['textures']) or len(textures) > 8:
        raise ValueError('Texture identity/budget mismatch')
    masks = {i: Mask(v, base.parent) for i, v in textures.items()}
    rows = {(r['frame'], r['ordinal']): r for r in census['draws']}
    if len(rows) != len(census['draws']):
        raise ValueError('Duplicate native draw observation')
    counts = []
    for fidx, frame in enumerate(frames, 1):
        totals = Counter(draws=len(frame.draws), quads=len(frame.quads) // 96)
        reasons = Counter()
        for ordinal, draw in enumerate(frame.draws):
            count, offset = draw[10], draw[11]
            if not count:
                continue
            row = rows.get((fidx, ordinal))
            if row is None:
                reasons['missing_draw_observation'] += count
                continue
            if row['count'] != count or int(row['context'], 16) != frame.context:
                raise ValueError('Native draw/state alignment mismatch')
            reason = state_reason(row)
            texture = textures.get(row.get('texture_snapshot'))
            if not reason and (not texture or texture['name'] != row['texture_name'] or texture.get('seen_frames') != 6 or not texture.get('bytes_identical_between_observed_frames')):
                reason = 'texture_snapshot_unavailable_or_unstable'
            if reason:
                reasons[reason] += count
                continue
            geometry, opaque, partial, zero_area = 0, 0, 0, 0
            for i in range(count):
                vertices = [VERTEX.unpack_from(frame.quads, (offset + i) * 96 + corner * 24) for corner in range(4)]
                if any((v[axis] & 0x7fffffff) > 0x4f800000 for v in vertices for axis in (0, 1, 2)) or any(not safe_depth(v[2], row['mvp_bits'][10]) for v in vertices):
                    reason = 'position_or_original_z_clip'
                    break
                geometry += 1
                positions = {(0 if (v[0] & 0x7fffffff) == 0 else v[0], 0 if (v[1] & 0x7fffffff) == 0 else v[1]) for v in vertices}
                if len(positions) == 1:
                    zero_area += 1
                else:
                    opaque += masks[row['texture_snapshot']].opaque(vertices)
                    partial += masks[row['texture_snapshot']].maybe_opaque(vertices)
            if reason:
                reasons[reason] += count
                continue
            totals['observed_state_candidate_draws'] += 1
            totals['observed_state_candidate_quads'] += geometry
            totals['nonzero_opaque_footprint_quads'] += opaque
            totals['nonzero_alpha_test_potential_quads'] += partial
            totals['point_degenerate_quads'] += zero_area
        counts.append(dict(native_frame=frame.number, percent=frame.percent, **totals, rejection_quads=dict(reasons)))
    programs = []
    for p in census['programs']:
        programs.append(dict(name=p['name'], frame=p['frame'], sources=[dict(type=s['type'], bytes=len(s['source']), sha256=hashlib.sha256(s['source'].encode()).hexdigest()) for s in p['attached_sources_not_linked_binary_proof']]))
    events = [json.loads(line) for line in base.read_text(encoding='utf-8').splitlines() if line.strip()]
    result = dict(diagnostic_only=True, game_fps_evidence=False, production_eligible=False,
                  limitation='Potential input coverage only. Linked shader semantics, framebuffer ownership, texture lifecycle beyond two frames and full game pixel/performance parity remain unproven.',
                  capture_sha256=hashlib.sha256(raw).hexdigest(), level=provenance['level'], measurement_sha256=provenance['measurement_sha256'],
                  deaths=sum(e.get('event') == 'death' for e in events), target_finished=any(e.get('event') == 'finished' and e.get('success') for e in events),
                  frames=counts, programs=programs, textures=[dict(index=i, sha256=m.sha256, width=m.width, height=m.height, distinct_footprint_queries=len(m.cache)) for i, m in masks.items()])
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_jsonl', type=Path)
    parser.add_argument('new_output', type=Path)
    args = parser.parse_args()
    result = analyze(args.native_jsonl)
    with args.new_output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({k: result[k] for k in ('diagnostic_only', 'production_eligible', 'deaths', 'target_finished', 'frames')}))
