"""Sabotage a PRIVATE GPU fixture copy, leaving the actual harness untouched."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
destination = root / "build-research" / "readback-gpu-mutant"
destination.mkdir(parents=True, exist_ok=True)
header = (root / "tests" / "FrameWatch.hpp").read_text(encoding="utf-8")
needle = "~PackBufferScope() noexcept { restore(); }"
if header.count(needle) != 1:
    raise SystemExit("Mutation site changed; refusing an ambiguous sabotage")
(destination / "FrameWatch.hpp").write_text(header.replace(needle, "~PackBufferScope() noexcept {}"), encoding="utf-8")
for name in ("VisualSignature.hpp", "visual_gpu_checks.cpp"):
    shutil.copyfile(root / "tests" / name, destination / name)
