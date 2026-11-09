"""Bounded copied movement inputs; never process access or native ownership proof."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct

HEADER = struct.Struct('<8s8IQIId')
ROW = struct.Struct('<3Q8I')
OBJECT = struct.Struct('<QIidd8I')
STATE = struct.Struct('<dd4I')
MAX_BYTES = HEADER.size + 8192 * ROW.size + 262144 * OBJECT.size


def decode(raw):
    if len(raw) < HEADER.size or len(raw) > MAX_BYTES:
        raise ValueError('Trace byte budget')
    h = HEADER.unpack_from(raw)
    magic, version, frames, rows, objects, row_bytes, object_bytes, reasons, features, owner, uid, reserved2, percent = h
    if (magic != b'CHLGTR63' or version != 63 or frames not in (1, 2) or rows > 8192 or objects > 262144 or
            row_bytes != ROW.size or object_bytes != OBJECT.size or reasons & ~15 or features & ~1 or reserved2 or
            not owner or not uid or not 0 <= percent <= 100 or
            len(raw) != HEADER.size + rows * ROW.size + objects * OBJECT.size):
        raise ValueError('Trace header or exact length')
    records = []
    offset = HEADER.size + rows * ROW.size
    for i in range(objects):
        record = OBJECT.unpack_from(raw, offset + i * OBJECT.size)
        if not record[0] or record[8] & ~63 or record[11] & ~1023:
            raise ValueError('Object identity or known state flags')
        records.append(record)
    calls = []
    consumed, previous_frame = 0, 1
    terminal_frames = set()
    for i in range(rows):
        row = ROW.unpack_from(raw, HEADER.size + i * ROW.size)
        array, dx, dy, frame, array_uid, stamp, count, first, observed, flags, mxcsr = row
        low_flags, snapshot = flags & 31, bool(flags & 32)
        if (not previous_frame <= frame <= frames or first != consumed or observed > count or count > 1000000 or
                observed > objects - consumed or not low_flags or flags & ~63 or mxcsr & ~65535 or
                (flags & 1 and flags & 12) or (not flags & 1 and not reasons) or
                (flags & 16 and (low_flags != 17 or array or count or observed)) or
                (flags & 2 and (not flags & 1 or observed >= count)) or
                (low_flags == 1 and (not array or (not snapshot and observed != count))) or
                frame in terminal_frames or (snapshot and not features & 1)):
            raise ValueError('Row boundaries, status or temporal order')
        if snapshot:
            terminal_frames.add(frame)
        calls.append(row)
        consumed += observed
        previous_frame = frame
    if consumed != objects:
        raise ValueError('Unreferenced object records')
    if features & 1 and not reasons and terminal_frames != set(range(1, frames + 1)):
        raise ValueError('Missing declared terminal frame observation')
    return h, calls, records


def bounded(value):
    bits = struct.unpack('<Q', struct.pack('<d', value))[0] & 0x7fffffffffffffff
    return bits == 0 or 0x3df0000000000000 <= bits <= 0x41f0000000000000


def predict(state, dx, dy, stamp, mxcsr):
    x, y, lx, ly, previous_stamp, flags = STATE.unpack(state)
    if mxcsr & 0x7f80 != 0x1f80 or not all(bounded(v) for v in (x, y, dx, dy)):
        return None
    if not flags & 1 and previous_stamp != stamp:
        lx, ly = struct.unpack('<2I', struct.pack('<2f', x, y))
        previous_stamp = stamp
        flags |= 12
    if dx != 0 and not flags & 2:
        x = dx + x
    if dy != 0:
        y = dy + y
    return STATE.pack(x, y, lx, ly, previous_stamp, flags | 48)


def analyze(raw):
    header, rows, objects = decode(raw)
    pending, metadata = {}, {}
    counts, fields = Counter(), Counter()
    unique_per_frame = defaultdict(dict)
    identities, terminal_keys, compared_keys, blocked_pose = set(), set(), set(), set()
    for ordinal, row in enumerate(rows):
        array, dx, dy, frame, array_uid, stamp, count, first, observed, flags, mxcsr = row
        snapshot = bool(flags & 32)
        if snapshot:
            counts['end_frame_snapshots'] += 1
        if not flags & 1:
            counts['incomplete_calls'] += 1
            pending.clear()
            continue
        if not observed:
            if not snapshot:
                counts['empty_calls'] += 1
            continue
        if not snapshot:
            counts['active_calls'] += 1
            counts['move_member_visits'] += observed
        dx, dy = (struct.unpack('<d', struct.pack('<Q', v))[0] for v in (dx, dy))
        before = {}
        records = objects[first:first + observed]
        for record in records:
            key = record[:2]
            state = STATE.pack(*record[3:9])
            if not snapshot:
                counts['member_visits_with_sections'] += record[2] >= 0
            identities.add(key)
            if snapshot and frame == header[2]:
                terminal_keys.add(key)
            if key in before:
                if before[key] != state:
                    raise ValueError('Duplicate identity changed during copied pre-call observation')
                counts['duplicate_member_visits'] += 1
            else:
                before[key] = state
                if pending.get(key) is not None:
                    compared_keys.add(key)
                    counts['reobserved_transitions'] += 1
                    previous = pending[key]
                    counts['position_matches'] += previous[:16] == state[:16]
                    counts['full_state_matches'] += previous == state
                    if previous[:16] != state[:16] or (STATE.unpack(previous)[5] ^ record[8]) & 3:
                        blocked_pose.add(key)
                    if snapshot:
                        counts['end_frame_transitions'] += 1
                        counts['end_frame_position_matches'] += previous[:16] == state[:16]
                        counts['end_frame_full_state_matches'] += previous == state
                    for name, start, end in [('position', 0, 16), ('last_position', 16, 24), ('stamp', 24, 28), ('flags', 28, 32)]:
                        fields[name] += previous[start:end] != state[start:end]
                pending[key] = state
            if not snapshot:
                unique_per_frame[frame].setdefault(key, []).append(ordinal)
            metadata[key] = record[9:13]
        # Repeated array entries execute repeatedly from the same pre-call snapshot.
        for record in ([] if snapshot else records):
            key = record[:2]
            if pending[key] is not None:
                pending[key] = predict(pending[key], dx, dy, stamp, mxcsr)
                counts['unsupported_arithmetic_visits'] += pending[key] is None
                if pending[key] is None:
                    blocked_pose.add(key)
    roles = Counter()
    for object_type, object_id, flags, group_count in metadata.values():
        for name, bit in [('decoration_type', 9), ('area_parent', 0), ('group_parent', 1), ('animated_child', 6)]:
            roles[name] += bool(flags & (1 << bit))
    complete = header[2] == 2 and not header[7] and bool(rows)
    return {
        'diagnostic_only': True, 'game_fps_evidence': False, 'native_reader_closure_proven': False,
        'sha256': hashlib.sha256(raw).hexdigest(), 'frames': header[2], 'capture_reasons': header[7],
        'complete_observation': complete, 'features': header[8],
        'percent_start': header[-1], 'calls': len(rows), 'member_visits': counts['move_member_visits'], 'state_records': len(objects), 'unique_objects': len(identities),
        'counts': dict(counts), 'discontinuity_fields': dict(fields), 'roles_objects': dict(roles),
        'final_observation_objects': len(terminal_keys),
        'unverified_final_states': len(identities - terminal_keys),
        'observed_movement_pose_consistent_objects': len((terminal_keys & compared_keys) - blocked_pose) if complete else 0,
        'per_frame': {str(frame): {
            'distinct_membership_programs': len({tuple(program) for program in members.values()}),
            'max_member_visits': max(map(len, members.values()), default=0)} for frame, members in unique_per_frame.items()},
        'limits': 'Observed pose continuity is not CPU-reader ownership. Discontinuities may be valid native writers/callbacks; missing final observations remain unqualified.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    args = parser.parse_args()
    if args.trace.stat().st_size > MAX_BYTES:
        raise SystemExit('Trace exceeds the input budget')
    print(json.dumps(analyze(args.trace.read_bytes()), indent=2))
