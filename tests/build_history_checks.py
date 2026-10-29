from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile

spec = importlib.util.spec_from_file_location(
    "build_history", Path(__file__).resolve().parents[1] / "research/R4-04/build_history.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    assert module.preserve_previous(root) is None
    original = b"previous DLL"
    (root / "local.chlorium.dll").write_bytes(original)
    (root / "source-inputs.zip").write_bytes(b"previous source archive")
    (root / "local.chlorium.audit.geode").write_bytes(b"older audit package")
    (root / "private.local.txt").write_text("must not be copied")
    first = module.preserve_previous(root)
    (root / "local.chlorium.dll").write_bytes(b"next DLL")
    second = module.preserve_previous(root)
    assert first != second
    assert (first / "local.chlorium.dll").read_bytes() == original
    assert (second / "local.chlorium.dll").read_bytes() == b"next DLL"
    assert not (first / "private.local.txt").exists()
    receipt = json.loads((first / "receipt.json").read_text())
    assert receipt["sha256"]["local.chlorium.dll"] == hashlib.sha256(original).hexdigest()
    assert (first / "local.chlorium.audit.geode").read_bytes() == b"older audit package"
print("PASS build/package history preserves bytes, hashes and separate revisions; excludes private logs")
