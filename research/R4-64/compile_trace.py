"""Compile a closed two-frame movement replay, never native GPU ownership."""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'R4-63'))
from analyze_trace import decode, apply_move, bounded, STATE, MAX_BYTES

PACKET = struct.Struct('<8s8I')
FRAME = struct.Struct('<4I')
COMMAND = struct.Struct('<dd4I')
RANGE = struct.Struct('<2I')
UNSUPPORTED_ROLES = 1 | 2 | 4 | 8 | 16 | 32 | 64


def advance(state, dx, dy, stamp, mxcsr):
    x, y, *_ = STATE.unpack(state)
    if (mxcsr & 0x7f80 != 0x1f80 or not bounded(dx) or not bounded(dy) or
            any(not math.isfinite(v) or abs(v)>2**32 or not math.ldexp(v,84).is_integer() for v in (x,y))):
        return None
    result = apply_move(state, dx, dy, stamp)
    return result if all(abs(v)<=2**32 for v in STATE.unpack(result)[:2]) else None


def compile_packet(raw):
    header, rows, objects = decode(raw)
    if header[2] != 2 or header[7] or not header[8] & 1:
        raise ValueError('Complete trace with two final observations required')
    seeds, pending, first_frame, roles, definitions = {}, {}, {}, {}, {}
    blocked = set()
    programs = {1: defaultdict(list), 2: defaultdict(list)}
    finals = {1: {}, 2: {}}
    for ordinal, row in enumerate(rows):
        _, dx_bits, dy_bits, frame, _, stamp, _, first, observed, flags, mxcsr = row
        if not flags & 1:
            raise ValueError('Incomplete call cannot become a replay')
        snapshot = bool(flags & 32)
        dx, dy = (struct.unpack('<d', struct.pack('<Q', value))[0] for value in (dx_bits, dy_bits))
        records = objects[first:first + observed]
        before = {}
        for record in records:
            key = record[:2]
            state = STATE.pack(*record[3:9])
            if key in before and before[key] != state:
                raise ValueError('Duplicate pre-call identity changed state')
            before[key] = state
            roles[key] = roles.get(key, 0) | record[11]
            if not record[11] & 512 or record[12] > 10 or (key in definitions and definitions[key] != record[9:11]):
                blocked.add(key)
            definitions[key] = record[9:11]
            if key not in seeds:
                seeds[key] = state;pending[key] = state;first_frame[key] = frame
                if not all(bounded(v) for v in record[3:5]):blocked.add(key)
            expected = pending.get(key)
            if expected is None or expected[:16] != state[:16] or (STATE.unpack(expected)[5] ^ record[8]) & 3:
                blocked.add(key)
            if snapshot:
                finals[frame][key] = state
            else:
                programs[frame][key].append(ordinal)
        # All duplicate entries were sampled before the original call; apply them in native order.
        if not snapshot:
            for record in records:
                key = record[:2]
                if pending[key] is not None:
                    pending[key] = advance(pending[key], dx, dy, stamp, mxcsr)
                    if pending[key] is None:
                        blocked.add(key)
    cohort = sorted(key for key in seeds if first_frame[key] == 1 and key in finals[1] and key in finals[2] and
                    key not in blocked and roles[key] & 512 and not roles[key] & UNSUPPORTED_ROLES and
                    all(len(programs[frame][key]) <= 64 for frame in (1, 2)))
    if not cohort or len(cohort) > 65536:
        raise ValueError('No bounded closed root-movement cohort')
    remaps, commands = {}, {}
    for frame in (1, 2):
        used = sorted({ordinal for key in cohort for ordinal in programs[frame][key]})
        if len(used) > 8192:
            raise ValueError('Global command budget')
        remaps[frame] = {ordinal: index for index, ordinal in enumerate(used)}
        commands[frame] = []
        for ordinal in used:
            row = rows[ordinal]
            dx, dy = (struct.unpack('<d', struct.pack('<Q', value))[0] for value in row[1:3])
            commands[frame].append((dx, dy, row[5], 0, 0, 0))
    classes, class_ids, actor_classes = [], {}, []
    for key in cohort:
        pair = tuple(tuple(remaps[frame][ordinal] for ordinal in programs[frame][key]) for frame in (1, 2))
        if pair not in class_ids:
            class_ids[pair] = len(classes);classes.append(pair)
        actor_classes.append(class_ids[pair])
    if len(classes) > 65536:
        raise ValueError('Program class budget')
    output = bytearray(PACKET.pack(b'CHLPRG64', 64, len(cohort), len(classes), 2, 32, 32, 8, 4))
    for key, program_class in zip(cohort, actor_classes):
        values = list(STATE.unpack(seeds[key]));values[5] |= program_class << 16
        output += STATE.pack(*values)
    frames = []
    for frame in (1, 2):
        ranges, indices, shared = [], [], {}
        for pair in classes:
            program = pair[frame - 1]
            if program not in shared:
                shared[program] = (len(indices), len(program));indices.extend(program)
            ranges.append(shared[program])
        output += FRAME.pack(len(commands[frame]), len(indices), len(cohort), 0)
        output += b''.join(COMMAND.pack(*command) for command in commands[frame])
        output += b''.join(RANGE.pack(*item) for item in ranges)
        output += struct.pack('<' + str(len(indices)) + 'I', *indices)
        output += b''.join(finals[frame][key][:16] for key in cohort)
        frames.append({'commands': len(commands[frame]), 'shared_indices': len(indices),
                       'max_steps': max((item[1] for item in ranges), default=0),
                       'program_upload_bytes': len(commands[frame])*32 + len(ranges)*8 + len(indices)*4})
    return bytes(output), {
        'diagnostic_only': True, 'native_reader_closure_proven': False, 'game_fps_evidence': False,
        'source_sha256': hashlib.sha256(raw).hexdigest(), 'observed_objects': len(seeds),
        'selected_root_objects': len(cohort), 'joint_program_classes': len(classes), 'frames': frames,
        'state_seed_bytes': len(cohort)*32,
        'limits': 'Observed movement poses only. No geometry, parent, CPU-reader or gameplay replacement qualification.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path);parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.trace.stat().st_size > MAX_BYTES:
        raise SystemExit('Trace exceeds input budget')
    if args.output.exists() or Path(str(args.output)+'.json').exists():
        raise SystemExit('Refusing an existing packet or receipt')
    packet, report = compile_packet(args.trace.read_bytes())
    report['packet_sha256'] = hashlib.sha256(packet).hexdigest()
    with args.output.open('xb') as stream:stream.write(packet)
    with Path(str(args.output)+'.json').open('x', encoding='utf-8') as stream:json.dump(report, stream, indent=2)
    print(json.dumps(report, indent=2))
