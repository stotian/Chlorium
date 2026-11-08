"""Bind one actual native batch/pixel pair to a bounded standalone replay file."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'research/R4-32'))
from analyze_capture import decode
from analyze_inputs import state_reason

SOURCE_SHA = {
    35633: '8f50cfb6a5577a4c446c2280aaf6c5ee110fc0a857fad146d09554af640fadd1',
    35632: '66c8bcab629b0c32ec1e5abe805b63b4f8ee49122c677e533d05ec925ef05e70',
}
HEADER = struct.Struct('<8s15I4i5I16I')


def local_bytes(directory, name, expected):
    if not isinstance(name, str) or Path(name).name != name or '/' in name or '\\' in name:
        raise ValueError('Replay inputs must use local basenames')
    path = directory / name
    if path.resolve().parent != directory.resolve() or path.stat().st_size != expected:
        raise ValueError('Replay input size/path differs from the native receipt')
    return path.read_bytes()


def export(base, destination):
    provenance = json.loads(Path(str(base) + '.provenance.json').read_text(encoding='utf-8-sig'))
    if not all(provenance.get(k) is True for k in ('native', 'native_pass_capture_diagnostic', 'opaque_input_census_diagnostic', 'native_batch_pixel_pair_diagnostic')):
        raise ValueError('Expected explicit isolated native batch-pair provenance')
    pair = json.loads(Path(str(base) + '.batch-pixels.json').read_text())
    if not pair.get('diagnostic_only') or not pair.get('valid_changed_pixels') or pair.get('fps_evidence') is not False:
        raise ValueError('No valid non-vacuous native pixel pair')
    census = json.loads(Path(str(base) + '.opaque-inputs.json').read_text())
    if census.get('failed'):
        raise ValueError('Census failed')
    frames = decode(Path(str(base) + '.pass-capture.bin').read_bytes())
    if len(frames) != 2 or any(f.reasons for f in frames):
        raise ValueError('Incomplete native frames')
    frame_id, ordinal = pair['frame'], pair['ordinal']
    if frame_id not in (1, 2) or not 0 <= ordinal < len(frames[frame_id-1].draws):
        raise ValueError('Native batch index out of range')
    frame = frames[frame_id-1]
    draw = frame.draws[ordinal]
    matches = [r for r in census['draws'] if (r['frame'], r['ordinal']) == (frame_id, ordinal)]
    if len(matches) != 1 or matches[0]['count'] != draw[10] or pair['count'] != draw[10]:
        raise ValueError('Native batch/state/pixel alignment mismatch')
    row = matches[0]
    if state_reason(row) or row['blend_src_rgb'] != 1 or row['blend_src_alpha'] != 1:
        raise ValueError('Native pair lies outside the replay state contract')
    definition = [p for p in census['programs'] if (p['frame'], p['name']) == (frame_id, row['program'])]
    if len(definition) != 1:
        raise ValueError('Missing native shader definition')
    shaders = {s['type']: s['source'].encode('utf-8') for s in definition[0]['attached_sources_not_linked_binary_proof']}
    if set(shaders) != set(SOURCE_SHA) or any(hashlib.sha256(shaders[k]).hexdigest() != v for k, v in SOURCE_SHA.items()):
        raise ValueError('Native shader source pair has not been reviewed')
    textures = [t for t in census['textures'] if t['index'] == row.get('texture_snapshot')]
    if len(textures) != 1 or textures[0]['seen_frames'] != 6 or not textures[0]['bytes_identical_between_observed_frames']:
        raise ValueError('Texture snapshot is missing or changes between frames')
    texture = textures[0]
    w, h = pair['width'], pair['height']
    tw, th = texture['width'], texture['height']
    if not 0 < w <= 1920 or not 0 < h <= 1080 or not 0 < tw <= 4096 or not 0 < th <= 4096 or pair['alpha_bits'] not in (0, 8):
        raise ValueError('Replay image dimensions/format unsupported')
    if row['min_filter'] != 0x2601 or row['mag_filter'] != 0x2601:
        raise ValueError('This replay currently requires the observed linear sampler')
    rgba = local_bytes(base.parent, texture['file'], tw*th*4)
    before = local_bytes(base.parent, pair['before_file'], w*h*4)
    after = local_bytes(base.parent, pair['after_file'], w*h*4)
    if before == after:
        raise ValueError('Vacuous native framebuffer pair')
    count, offset = draw[10], draw[11]
    quads = bytes(frame.quads[offset*96:(offset+count)*96])
    binary = b''
    binary_format = 0
    if '.batch-vbo.bin' in pair:
        vertices = local_bytes(base.parent, pair['.batch-vbo.bin'], count*96)
        if vertices != quads or pair.get('gpu_vertices_match_cpu') is not True or pair.get('first') != 0:
            raise ValueError('Actual native GPU vertices differ from the supported CPU batch')
        indices = local_bytes(base.parent, pair['.batch-ibo.bin'], count*12)
        expected_indices = b''.join(struct.pack('<6H', *(i*4+j for j in (0, 1, 2, 3, 2, 1))) for i in range(count))
        if indices != expected_indices:
            raise ValueError('Actual native GPU indices differ from the reviewed topology')
        if '.batch-program.bin' in pair:
            binary_path = base.parent / pair['.batch-program.bin']
            if binary_path.name != pair['.batch-program.bin'] or binary_path.resolve().parent != base.parent.resolve():
                raise ValueError('Linked program must be a local native receipt')
            if not 0 < binary_path.stat().st_size <= 4*1024*1024:
                raise ValueError('Native linked program exceeds replay budget')
            binary = local_bytes(base.parent, pair['.batch-program.bin'], binary_path.stat().st_size)
            binary_format = pair['program_binary_format']
    flags = bool(pair['dither']) | (bool(pair['cull_face']) << 1) | (bool(pair['scissor']) << 2)
    mask = sum(bool(value) << i for i, value in enumerate(pair['color_mask']))
    header = HEADER.pack(b'CHLPIX41', 2, w, h, pair['alpha_bits'], flags, pair['cull_mode'], pair['front_face'], pair['framebuffer'],
                         pair['alpha_func'], pair['alpha_ref_bits'], pair['color_encoding'], row['internal_format'], mask,
                         bool(pair['legacy_alpha_test']), bool(pair['framebuffer_srgb']), *pair['scissor_box'],
                         count, tw, th, len(shaders[35633]), len(shaders[35632]), *row['mvp_bits'])
    data = header + struct.pack('<2I', binary_format, len(binary)) + shaders[35633] + shaders[35632] + binary + rgba + quads + before + after
    if len(data) > 64*1024*1024:
        raise ValueError('Bounded replay file exceeded')
    with destination.open('xb') as stream:
        stream.write(data)
    metadata = dict(diagnostic_only=True, game_fps_evidence=False, level=provenance['level'], frame=frame.number, percent=frame.percent,
                    ordinal=ordinal, quads=count, width=w, height=h, source_measurement_sha256=provenance['measurement_sha256'],
                    dataset_sha256=hashlib.sha256(data).hexdigest(), before_sha256=hashlib.sha256(before).hexdigest(), after_sha256=hashlib.sha256(after).hexdigest())
    with Path(str(destination)+'.json').open('x', encoding='utf-8') as stream:
        json.dump(metadata, stream, indent=2)
    return metadata


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native_jsonl', type=Path)
    parser.add_argument('new_replay', type=Path)
    args = parser.parse_args()
    print(json.dumps(export(args.native_jsonl, args.new_replay)))
