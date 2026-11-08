"""Bounded x64 unwind ownership, including split/chained native procedures."""
import struct


def function_ranges(pe):
    image = pe.get_memory_mapped_image()
    if pe.FILE_HEADER.Machine != 0x8664:
        raise ValueError("Unwind ownership requires an x64 image")
    entries = [entry.struct for entry in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", [])]
    owners = {}

    def read(rva, layout):
        size = struct.calcsize(layout)
        if rva < 0 or rva > len(image) or size > len(image) - rva:
            raise ValueError("Unwind metadata outside image")
        return struct.unpack_from(layout, image, rva)

    def owner(record):
        pending, seen = [], set()
        while record not in owners:
            begin, end, unwind = record
            if not 0 <= begin < end <= len(image) or record in seen or len(pending) >= 64:
                raise ValueError("Invalid or cyclic unwind ownership")
            seen.add(record)
            pending.append(record)
            if unwind & 1:
                record = read(unwind & ~1, "<III")
                continue
            if unwind & 3:
                raise ValueError("Unaligned x64 unwind metadata")
            version_flags, _, count, _ = read(unwind, "<BBBB")
            version, flags = version_flags & 7, version_flags >> 3
            if version not in (1, 2) or flags & ~7:
                raise ValueError("Unsupported x64 unwind format; no owner inferred")
            code_end = (unwind + 4 + count * 2 + 3) & ~3
            read(unwind + 4, f"<{code_end - unwind - 4}s")
            if flags & 4:
                if flags & 3:
                    raise ValueError("Chained unwind has conflicting handler flags")
                record = read(code_end, "<III")
                continue
            if flags & 3:
                read(code_end, "<I")
            owners[record] = begin
        root = owners[record]
        for item in pending:
            owners[item] = root
        return root

    ranges = sorted(set((entry.BeginAddress, entry.EndAddress,
                         owner((entry.BeginAddress, entry.EndAddress, entry.UnwindData))) for entry in entries))
    for left, right in zip(ranges, ranges[1:]):
        if left[1] > right[0]:
            raise ValueError("Overlapping x64 unwind ranges")
    return ranges
