"""Create only local research fixtures from a verified manual build fingerprint.
This does not export to dist, install into a game, or publish a release.
"""
from pathlib import Path
import hashlib
import json
import sys
import zipfile

root = Path(__file__).resolve().parents[2]
profile = sys.argv[1] if len(sys.argv) == 2 else ""
if profile not in ("quad-v4", "quad-v4-scope-r2", "quad-audit-r3", "native-vertex-black-v5", "measurement-v5", "measurement-audit-r2", "measurement-black-watch-v5", "measurement-area-census-v5", "measurement-area-visible-v5", "measurement-area-frames-v5", "measurement-pose-census-v6"):
    raise SystemExit("Choose an explicit compiled research profile")
out = root / "build-research" / profile
build = json.loads((out / "build-fingerprint.json").read_text(encoding="utf-8"))
if build["compile_status"] != "passed" or build["link_status"] != "passed":
    raise SystemExit("No successful build")
dll = out / "local.chlorium.dll"
binary = dll.read_bytes()
if hashlib.sha256(binary).hexdigest() != build["dll_sha256"]:
    raise SystemExit("DLL fingerprint mismatch")
if build.get("source_archive_sha256"):
    if hashlib.sha256((out / "source-inputs.zip").read_bytes()).hexdigest() != build["source_archive_sha256"]:
        raise SystemExit("Source archive fingerprint mismatch")
audit = profile.startswith("measurement-")
expected = (b"CHLORIUM_RUNTIME_AUDIT_BINARY_V1", b"CHLORIUM_READBACK_STATE_GUARDS_V5") if audit else (
    b"CHLORIUM_NORMAL_BINARY_V1", b"CHLORIUM_NATIVE_VERTEX_BACKEND_V5" if profile=="native-vertex-black-v5" else b"CHLORIUM_NATIVE_QUAD_BACKEND_V4", b"CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1")
if not all(marker in binary for marker in expected):
    raise SystemExit("Wrong binary identity")
for resource in build["resources"]:
    if hashlib.sha256((root / resource["path"]).read_bytes()).hexdigest() != resource["sha256"]:
        raise SystemExit("Resource changed after build: " + resource["path"])
manifest = json.loads((root / "mod.json").read_text(encoding="utf-8"))
manifest["id"] = build["mod_id"]
manifest["version"] = build["version"]
manifest["name"] = "Chlorium independent measurement V5" if audit else "Chlorium" if profile=="native-vertex-black-v5" else "Chlorium native quads V4 research"
manifest["settings"].pop("ingame_enable", None)
manifest["settings"].pop("use_mapped_buffers", None)
package = out / (manifest["id"] + ".geode")
with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("mod.json", json.dumps(manifest, ensure_ascii=False, indent=2))
    archive.writestr(manifest["id"] + ".dll", binary)
    for name in ("about.md", "changelog.md", "support.md", "logo.png", "LICENSE.md", "COPYING", "NOTICE.md", "THIRD_PARTY_NOTICES.md"):
        archive.write(root / name, name)
    for source in sorted((root / "LICENSES").rglob("*")):
        if source.is_file():
            archive.write(source, source.relative_to(root).as_posix())
    # Geode flattens resource names using the mod-ID prefix, as package-local.ps1.
    names = set()
    for resource in build["resources"]:
        source = root / resource["path"]
        name = "resources/" + manifest["id"] + "/" + source.name
        if name in names:
            raise SystemExit("Ambiguous resource basename: " + source.name)
        names.add(name)
        archive.write(source, name)
with zipfile.ZipFile(package) as verify:
    if verify.testzip() is not None:
        raise SystemExit("Package CRC check failed")
    required = ["mod.json", manifest["id"] + ".dll", "COPYING", "LICENSE.md",
                "resources/" + manifest["id"] + "/nativeQuad.vert",
                "resources/" + manifest["id"] + "/shared.h"]
    if not all(name in verify.namelist() for name in required):
        raise SystemExit("Package resources incomplete")
result = {"profile": profile, "package": package.name, "sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
          "dll_sha256": build["dll_sha256"], "game_test": "not_run", "installed": False, "published": False}
(out / "package-fingerprint.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result))
