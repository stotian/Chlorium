"""Preserve existing evidence before a build or package overwrites it."""
from pathlib import Path
import hashlib
import json
import shutil
import uuid


def preserve_previous(out: Path):
    names = ("build-fingerprint.json", "package-fingerprint.json", "source-inputs.zip",
             "local.chlorium.dll", "local.chlorium.geode", "local.chlorium.audit.geode")
    files = [out / name for name in names if (out / name).is_file()]
    if not files:
        return None
    history = out / "history" / uuid.uuid4().hex
    history.mkdir(parents=True)
    hashes = {}
    for source in files:
        destination = history / source.name
        shutil.copy2(source, destination)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
            raise RuntimeError("Evidence copy failed: " + source.name)
        hashes[source.name] = digest
    (history / "receipt.json").write_text(json.dumps({
        "sha256": hashes,
        "note": "Existing bytes only; build/package revisions may already differ."
    }, indent=2), encoding="utf-8")
    return history
