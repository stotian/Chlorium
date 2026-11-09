"""Create only local research fixtures from a verified manual build fingerprint.
This does not export to dist, install into a game, or publish a release.
"""
from pathlib import Path
import hashlib
import json
import sys
import zipfile
from build_history import preserve_previous

root = Path(__file__).resolve().parents[2]
profile = sys.argv[1] if len(sys.argv) == 2 else ""
if profile not in ("native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63", "native-resident-fast-r58", "native-resident-continuous-r57", "native-opacity-hash-r36", "measurement-native-pass-v10", "measurement-canonical-v10-r4","measurement-resident-continuous-r57","measurement-resident-deferred-r59", "measurement-resident-timed-r60", "measurement-group-trace-r63", "measurement-canonical-v10-r3", "measurement-canonical-opacity-shadow-v10", "measurement-canonical-sampling-v10", "measurement-canonical-v10-r2", "measurement-canonical-v10", "native-canonical-v10", "quad-v4", "quad-v4-scope-r2", "quad-audit-r3", "native-vertex-black-v5", "measurement-v5", "measurement-audit-r2", "measurement-black-watch-v5", "measurement-area-census-v5", "measurement-area-visible-v5", "measurement-area-frames-v5", "measurement-pose-census-v6", "native-adaptive-upload-v6", "measurement-upload-v6", "native-draw-bridge-v7", "native-gpu-exchange-v8", "native-persistent-transfer-v9", "measurement-transfer-v9", "measurement-transfer-v9-r2"):
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
if (profile=="measurement-group-trace-r63") != (b"CHLORIUM_GROUP_TRACE_AUDIT_R63" in binary):
    raise SystemExit("R63 auditor identity/profile mismatch")
if (profile=="native-group-trace-r63") != (b"CHLORIUM_GROUP_STATE_TRACE_R63" in binary):
    raise SystemExit("R63 group trace identity/profile mismatch")
if (profile in ("native-stream-direct-r61", "native-group-trace-r63")) != (b"CHLORIUM_NATIVE_STREAM_DIRECT_R61" in binary):
    raise SystemExit("R61 native stream identity/profile mismatch")
if profile in ("measurement-resident-timed-r60", "measurement-group-trace-r63") and b"CHLORIUM_RENDERER_TIMING_AUDIT_R60" not in binary:
    raise SystemExit("R60 timing telemetry missing")
if profile in ("native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63") and b"CHLORIUM_RENDERER_TIMING_R60" not in binary:
    raise SystemExit("R60 timing identity missing")
if profile=="measurement-resident-deferred-r59" and b"CHLORIUM_RESIDENT_TELEMETRY_R59" not in binary:
    raise SystemExit("R59 measurement telemetry missing")
expected = (b"CHLORIUM_RUNTIME_AUDIT_BINARY_V1", b"CHLORIUM_READBACK_STATE_GUARDS_V5") if audit else (
    b"CHLORIUM_NORMAL_BINARY_V1", b"CHLORIUM_NATIVE_VERTEX_BACKEND_V5" if profile in ("native-vertex-black-v5","native-adaptive-upload-v6","native-draw-bridge-v7", "native-gpu-exchange-v8", "native-persistent-transfer-v9") else b"CHLORIUM_NATIVE_QUAD_BACKEND_V4", b"CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1")
if profile in ("native-resident-continuous-r57", "native-resident-fast-r58", "native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
    expected=(b"CHLORIUM_NORMAL_BINARY_V1",b"CHLORIUM_RESIDENT_CONTINUOUS_R57")
    if profile in ("native-resident-fast-r58", "native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
        expected += (b"CHLORIUM_RESIDENT_FAST_R58",)
    elif b"CHLORIUM_RESIDENT_FAST_R58" in binary:
        raise SystemExit("R58 prototype cannot be packaged as R57")
    if profile in ("native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
        expected += (b"CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59",)
    elif b"CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59" in binary:
        raise SystemExit("R59 prototype cannot be packaged as an earlier renderer")
    if any(marker in binary for marker in (b"CHLORIUM_NATIVE_CANONICAL_BACKEND_V10",b"CHLORIUM_RUNTIME_AUDIT_BINARY_V1",b"CHLORIUM_NATIVE_PASS_CAPTURE_V3")):
        raise SystemExit("Old core or replay auditor in continuous renderer package")
if profile in ("native-canonical-v10", "native-opacity-hash-r36"):
    expected=(b"CHLORIUM_NORMAL_BINARY_V1",b"CHLORIUM_NATIVE_CANONICAL_BACKEND_V10",b"CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10")
    if any(marker in binary for marker in (b"CHLORIUM_NATIVE_VERTEX_BACKEND_V5",b"CHLORIUM_RUNTIME_AUDIT_BINARY_V1",b"CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1")):
        raise SystemExit("Retired or audit code in canonical candidate")
if profile=="native-opacity-hash-r36" and b"CHLORIUM_NATIVE_OPACITY_HASH_R36" not in binary:
    raise SystemExit("Opacity hash experiment identity missing")
if profile in ("native-adaptive-upload-v6","native-draw-bridge-v7", "native-gpu-exchange-v8") and b"CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6" not in binary:
    raise SystemExit("Adaptive upload identity missing")
if profile in ("native-draw-bridge-v7", "native-gpu-exchange-v8") and b"CHLORIUM_NATIVE_DRAW_BRIDGE_V7" not in binary:
    raise SystemExit("Native draw handoff identity missing")
if profile=="native-gpu-exchange-v8" and b"CHLORIUM_NATIVE_GPU_EXCHANGE_V8" not in binary:
    raise SystemExit("Native GPU exchange identity missing")
if profile=="native-persistent-transfer-v9" and b"CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9" not in binary:
    raise SystemExit("Persistent transfer identity missing")
if profile in ("measurement-canonical-v10","measurement-canonical-v10-r2","measurement-native-pass-v10", "measurement-canonical-v10-r4","measurement-resident-continuous-r57","measurement-resident-deferred-r59", "measurement-resident-timed-r60", "measurement-group-trace-r63", "measurement-canonical-v10-r3","measurement-canonical-sampling-v10","measurement-canonical-opacity-shadow-v10") and b"CHLORIUM_CANONICAL_CORE_AUDIT_V10" not in binary:
    raise SystemExit("Canonical core audit identity missing")
if profile in ("measurement-canonical-v10-r2","measurement-native-pass-v10", "measurement-canonical-v10-r4","measurement-resident-continuous-r57","measurement-resident-deferred-r59", "measurement-resident-timed-r60", "measurement-group-trace-r63", "measurement-canonical-v10-r3","measurement-canonical-sampling-v10","measurement-canonical-opacity-shadow-v10") and b"CHLORIUM_CANONICAL_AUDIT_BOUNDED_PE_R2" not in binary:
    raise SystemExit("Bounded PE audit identity missing")
if profile=="measurement-canonical-sampling-v10" and b"CHLORIUM_EXTERNAL_SAMPLING_AUDIT_V2" not in binary:
    raise SystemExit("External sampling audit identity missing")
if profile in ("measurement-canonical-v10-r3", "measurement-canonical-v10-r4") and b"CHLORIUM_AUDIT_SCENE_TRANSITION_GUARD_R3" not in binary:
    raise SystemExit("Scene transition guard identity missing")
if profile=="measurement-canonical-v10-r4" and b"CHLORIUM_AUDIT_120FPS_PACING_R4" not in binary:
    raise SystemExit("120 FPS pacing identity missing")
if (profile=="measurement-native-pass-v10") != (b"CHLORIUM_NATIVE_PASS_CAPTURE_V3" in binary):
    raise SystemExit("Native pass capture identity/profile mismatch")
if (profile=="measurement-canonical-opacity-shadow-v10") != (b"CHLORIUM_OPACITY_PREFIX_SHADOW_V1" in binary):
    raise SystemExit("Opacity shadow identity/profile mismatch")
if profile=="measurement-transfer-v9-r2" and b"CHLORIUM_TRANSFER_HYBRID_AUDIT_V9" not in binary:
    raise SystemExit("Typed hybrid validation identity missing")
if not all(marker in binary for marker in expected):
    raise SystemExit("Wrong binary identity")
for resource in build["resources"]:
    if hashlib.sha256((root / resource["path"]).read_bytes()).hexdigest() != resource["sha256"]:
        raise SystemExit("Resource changed after build: " + resource["path"])
manifest = json.loads((root / "mod.json").read_text(encoding="utf-8"))
manifest["id"] = build["mod_id"]
manifest["version"] = build["version"]
manifest["name"] = "Chlorium" if profile=="native-canonical-v10" else "Chlorium independent measurement V5" if audit else "Chlorium persistent transfer V9 research" if profile=="native-persistent-transfer-v9" else "Chlorium GPU exchange V8 research" if profile=="native-gpu-exchange-v8" else "Chlorium native handoff V7 research" if profile=="native-draw-bridge-v7" else "Chlorium adaptive upload V6 research" if profile=="native-adaptive-upload-v6" else "Chlorium" if profile=="native-vertex-black-v5" else "Chlorium native quads V4 research"
if profile=="native-opacity-hash-r36":
    manifest["name"]="Chlorium opacity hash R36 research"
if profile in ("native-resident-continuous-r57", "native-resident-fast-r58", "native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
    manifest["name"]="Chlorium continuous GPU renderer R58 (unvalidated research)" if profile=="native-resident-fast-r58" else "Chlorium continuous GPU renderer R57 (research)"
    manifest["description"]="Experimental continuous retained GPU rendering; native callbacks and preparation preserved; private validation build"
if profile in ("native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
    manifest["name"]="Chlorium deferred native uploads R59 (research)"
    manifest["description"]="Experimental GPU renderer with deferred native vertex uploads and native dirty-flag recovery; unqualified in game"
if profile in ("native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
    manifest["name"]="Chlorium native stream R61 (diagnostic)" if profile in ("native-stream-direct-r61", "native-group-trace-r63") else "Chlorium renderer timing R60 (diagnostic)"
if profile=="measurement-group-trace-r63":
    manifest["name"]="Chlorium R63 independent group trace auditor"
if profile=="native-group-trace-r63":
    manifest["name"]="Chlorium R61 renderer + R63 group state trace (diagnostic)"
    manifest["description"]="Two-frame native movement input trace with Chlorium R61 active; no GPU group offload or performance claim"
manifest["settings"].pop("ingame_enable", None)
manifest["settings"].pop("use_mapped_buffers", None)
package = out / (manifest["id"] + ".geode")
preserve_previous(out)
with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("mod.json", json.dumps(manifest, ensure_ascii=False, indent=2))
    archive.writestr(manifest["id"] + ".dll", binary)
    if profile in ("native-resident-continuous-r57", "native-resident-fast-r58", "native-resident-deferred-r59", "native-resident-timed-r60", "native-stream-direct-r61", "native-group-trace-r63"):
        with zipfile.ZipFile(out / "source-inputs.zip") as frozen:
            shader=frozen.read("research/R4-33/resident.vert")
        if hashlib.sha256(shader).hexdigest()!="bc8f9c88d908d769b440d86f10d494520a582b655f46de0c4348b7911cca9ad0":
            raise SystemExit("Unreviewed continuous shader")
        archive.writestr("resources/"+manifest["id"]+"/resident.vert",shader)
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
