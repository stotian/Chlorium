"""Resolve diagnostic leaf samples using explicitly fingerprinted local PE files.
No process access, symbols download or guessed nearest-export function names.
"""
import argparse
from bisect import bisect_right
from collections import Counter
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "build-research/python"))
import pefile
from pe_ranges import function_ranges


def image_map(record):
    raw = Path(record["path"]).read_bytes()
    if not re.fullmatch(r"[0-9a-fA-F]{64}", record["sha256"]) or hashlib.sha256(raw).hexdigest() != record["sha256"].lower():
        raise ValueError("Source image fingerprint mismatch")
    pe = pefile.PE(data=raw, fast_load=True)
    try:
        if pe.FILE_HEADER.Machine != 0x8664:
            raise ValueError("Expected x64 source image")
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"], pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
        exports = {}
        for symbol in getattr(getattr(pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", []):
            if symbol.name and not symbol.forwarder:
                exports[symbol.address] = symbol.name.decode("ascii", errors="replace")
        spans = function_ranges(pe)
        return pe.OPTIONAL_HEADER.SizeOfImage, exports, spans, [entry[0] for entry in spans]
    finally:
        pe.close()


def resolve(samples, metadata):
    if samples.stat().st_size > 5 * 1024 * 1024:
        raise ValueError("Diagnostic sample file exceeds budget")
    if metadata.get("diagnostic_only") is not True or metadata.get("auditor_session_verified") is not True or metadata.get("cleanup_failures") != 0:
        raise ValueError("Expected successful diagnostic metadata")
    if hashlib.sha256(samples.read_bytes()).hexdigest() != metadata["sample_file_sha256"]:
        raise ValueError("Sample file fingerprint mismatch")
    manifest = metadata["images"]
    images = {name.casefold(): image_map(record) for name, record in manifest.items()}
    counts = Counter()
    total = 0
    capture_ms = 0.0
    with samples.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            name = row["module"].casefold()
            if not re.fullmatch(r"[a-z0-9_.-]+", name) or not re.fullmatch(r"0x[0-9a-fA-F]+", row["rva"]):
                raise ValueError("Invalid sampled module/address")
            rva = int(row["rva"], 16)
            cost = float(row["snapshot_ms"])
            if not math.isfinite(cost) or cost < 0:
                raise ValueError("Invalid snapshot cost")
            capture_ms += cost
            start, kind, symbol = rva & ~63, "address_block_64_bytes", ""
            if name in images:
                size, exports, spans, begins = images[name]
                if rva >= size:
                    raise ValueError("Sample outside fingerprinted module")
                index = bisect_right(begins, rva) - 1
                if index >= 0 and rva < spans[index][1]:
                    start, kind = spans[index][2], "unwind_procedure_owner"
                    symbol = exports.get(start, "")
                elif rva in exports:
                    start, kind, symbol = rva, "exact_export_address", exports[rva]
            counts[(name, start, kind, symbol)] += 1
            total += 1
    if not total:
        raise ValueError("No diagnostic leaf samples")
    if total != metadata["samples"]:
        raise ValueError("Sample count differs from the capture receipt")
    return {"diagnostic_only": True, "samples": total, "snapshot_total_ms_in_rows": capture_ms,
            "scope": "Wall-clock leaf locations, including waits; no inclusive stacks or FPS verdict.",
            "sample_file_sha256": hashlib.sha256(samples.read_bytes()).hexdigest(),
            "images": manifest,
            "hotspots": [{"module": key[0], "start_rva": hex(key[1]), "resolution": key[2], "symbol": key[3],
                          "samples": count, "wall_sample_percent": 100 * count / total}
                         for key, count in counts.most_common()]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", type=Path)
    parser.add_argument("metadata", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = resolve(args.samples, json.loads(args.metadata.read_text(encoding="utf-8-sig")))
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
