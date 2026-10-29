"""Rebuild candidate sources serially; reuse fingerprinted local SDK/static libraries.
No CMake reconfiguration, package generation, installation, or game process.
"""
from pathlib import Path
import hashlib
import json
import re
import shlex
import subprocess
import sys
import zipfile
from build_history import preserve_previous

root = Path(__file__).resolve().parents[2]
profiles = {
    "": "native-vertex-black-v5",
    "--quad-v4": "quad-v4",
    "--measurement": "measurement-v5",
    "--measurement-r2": "measurement-audit-r2",
    "--native-audit": "native-static-analysis",
    "--native-vertex": "native-vertex-black-v5",
    "--black-watch": "measurement-black-watch-v5",
    "--scope-r2": "quad-v4-scope-r2",
    "--audit-r3": "quad-audit-r3",
    "--area-census": "measurement-area-census-v5",
    "--area-census-visible": "measurement-area-visible-v5",
    "--area-census-frames": "measurement-area-frames-v5",
    "--pose-census": "measurement-pose-census-v6",
    "--adaptive-upload": "native-adaptive-upload-v6",
    "--upload-measure": "measurement-upload-v6",
    "--native-bridge": "native-draw-bridge-v7",
    "--gpu-exchange": "native-gpu-exchange-v8",
    "--persistent-transfer": "native-persistent-transfer-v9",
    "--canonical": "native-canonical-v10",
    "--canonical-measure": "measurement-canonical-v10",
    "--transfer-measure": "measurement-transfer-v9",
    "--transfer-measure-r2": "measurement-transfer-v9-r2",
}
if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in profiles):
    raise SystemExit("Choose exactly one explicit research build profile")
selected = sys.argv[1] if len(sys.argv) == 2 else ""
profile = profiles[selected]
measurement = profile.startswith("measurement-")
native_audit = selected == "--native-audit"
canonical = profile == "native-canonical-v10"
native_persistent = profile == "native-persistent-transfer-v9"
native_vertex = canonical or native_persistent or profile in ("native-vertex-black-v5", "native-adaptive-upload-v6", "native-draw-bridge-v7", "native-gpu-exchange-v8")
native_exchange = profile == "native-gpu-exchange-v8"
native_bridge = profile in ("native-draw-bridge-v7", "native-gpu-exchange-v8")
adaptive_upload = profile in ("native-adaptive-upload-v6", "native-draw-bridge-v7", "native-gpu-exchange-v8")
visible_census = selected == "--area-census-visible"
frame_census = selected == "--area-census-frames"
area_census = selected in ("--area-census", "--area-census-visible", "--area-census-frames")
pose_census = selected == "--pose-census"
previous = Path("C:/Users/stoti/Chlorium/build")
ninja = (previous / "build.ninja").read_text(encoding="utf-8")
block = re.search(r"^build CMakeFiles\\Chlorium.dir\\src\\renderer\\NativeQuadRenderer.cpp.obj:.*?(?=^build |\Z)", ninja, re.M | re.S)
if not block:
    raise SystemExit("Missing validated local include configuration")
include_line = re.search(r"^  INCLUDES = (.+)$", block.group(), re.M).group(1)
includes = []
for argument in shlex.split(include_line, posix=False):
    if argument.startswith(("-I", "-external:I")):
        external = argument.startswith("-external:I")
        path = argument[len("-external:I") if external else 2:].strip('"')
        if Path(path) == previous.parent / "src":
            path = str(root / "src")
        includes.append(("/external:I" if external else "/I") + path)
flags = ["/nologo", "/c", "/std:c++latest", "/MD", "/EHsc", "/O2", "/Ob1", "/DNDEBUG", "/W3", "/external:W0", "/bigobj", "/utf-8",
         "/DWIN32", "/D_WINDOWS", "/DNOMINMAX", "/DGEODE_COMP_GD_VERSION=22081", "/DGEODE_GD_VERSION=2.2081",
         '/DGEODE_GD_VERSION_STRING="2.2081"', '/DGEODE_MOD_ID="local.chlorium"', "/DMAT_JSON_DYNAMIC=1",
         "/DARC_FEATURE_TIME=1", "/D_HAS_ITERATOR_DEBUGGING=0", "/DChlorium_EXPORTS", "/DPROJECT_NAME=Chlorium",
         "/DCHLORIUM_EXPERIMENTAL_NATIVE_QUADS=1", "/DCHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS=1", "/DCHLORIUM_NATIVE_SNAPSHOTS=1"]
source_names = ["src/renderer/NativeQuadRenderer.cpp", "src/renderer/Shader.cpp", "src/renderer/ShaderPreprocessor.cpp",
                "src/renderer/Buffer.cpp", "src/renderer/NativeAtlasStatsEmpty.cpp", "src/renderer/NativePauseGuard.cpp",
                "src/BuildIdentity.cpp", "src/common.cpp"]
if measurement:
    flags = [flag for flag in flags if not flag.startswith(("/DGEODE_MOD_ID=", "/DCHLORIUM_EXPERIMENTAL_NATIVE_", "/DCHLORIUM_NATIVE_SNAPSHOTS="))]
    flags += ['/DGEODE_MOD_ID="local.chlorium.audit"', "/DCHLORIUM_RUNTIME_AUDIT=1", "/DCHLORIUM_ORBIT_BENCHMARK=1", "/DCHLORIUM_MEASUREMENT_ONLY=1"]
    source_names = ["tests/NativeAtlasStatsStub.cpp", "src/BuildIdentity.cpp", "src/BProfiler.cpp", "tests/auto_level_benchmark.cpp"]
    if area_census:
        flags += ["/DCHLORIUM_AREA_WORK_CENSUS=1"]
        if visible_census:
            flags += ["/DCHLORIUM_AREA_CENSUS_VISIBLE_ONLY=1"]
        if frame_census:
            flags += ["/DCHLORIUM_AREA_CENSUS_FRAMES=1"]
        source_names.append("research/R4-08/AreaWorkCensus.cpp")
    if pose_census:
        flags += ["/DCHLORIUM_AREA_WORK_CENSUS=1", "/DCHLORIUM_POSE_CENSUS=1"]
        source_names.append("research/R4-11/PoseCensus.cpp")
if canonical:
    flags += ["/DCHLORIUM_NATIVE_CANONICAL_V10=1"]
    source_names = ["src/renderer/NativeAreaDivisions.cpp", "src/renderer/NativeAtlasStatsEmpty.cpp", "src/renderer/NativePauseGuard.cpp", "src/BuildIdentity.cpp"]
sources = [root / name for name in source_names]
if selected=="--canonical-measure":
    flags += ["/DCHLORIUM_CANONICAL_AUDIT_V10=1"]
if selected=="--transfer-measure-r2":
    flags += ["/DCHLORIUM_TRANSFER_HYBRID_AUDIT_V9=1"]
if native_persistent:
    flags += ["/DCHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9=1"]
if native_vertex:
    flags += ["/DCHLORIUM_NATIVE_VERTEX_PARITY=1"]
if adaptive_upload:
    flags += ["/DCHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6=1"]
if native_exchange:
    flags += ["/DCHLORIUM_NATIVE_GPU_EXCHANGE_V8=1"]
if native_bridge:
    flags += ["/DCHLORIUM_NATIVE_DRAW_BRIDGE_V7=1"]
if native_audit:
    flags = [flag for flag in flags if not flag.startswith(("/DCHLORIUM_EXPERIMENTAL_NATIVE_", "/DCHLORIUM_NATIVE_SNAPSHOTS="))]
    flags += ["/analyze"]
    source_names = ["legacy/chlorium/NativeTransformCache.cpp", "src/renderer/NativeAtlasStatsEmpty.cpp",
                    "src/renderer/NativePauseGuard.cpp", "src/BuildIdentity.cpp"]
    sources = [root / name for name in source_names]
sources.append(Path("C:/Users/stoti/Projects/gd-timing-lab/deps/geode-sdk/entry.cpp"))
libraries_text = re.search(r"^  LINK_LIBRARIES = (.+)$", ninja, re.M).group(1)
libraries = []
for name in shlex.split(libraries_text, posix=False):
    name = name.strip('"')
    path = Path(name)
    if path.is_absolute() or "\\" in name or "/" in name:
        path = path if path.is_absolute() else previous / path
        if not path.is_file():
            raise SystemExit(f"Missing dependency: {path.name}")
        libraries.append(str(path))
    else:
        libraries.append(name)
out = root / "build-research" / profile
out.mkdir(parents=True, exist_ok=True)
preserve_previous(out)
fingerprint = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
manifest = {"schema_version": 1, "revision": profile,
            "backend": "CHLORIUM_NATIVE_CANONICAL_BACKEND_V10" if canonical else "NATIVE_TRANSFORM_CACHE_STATIC_ANALYSIS" if native_audit else "CHLORIUM_READBACK_STATE_GUARDS_V5" if measurement else "CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9" if native_persistent else "CHLORIUM_NATIVE_GPU_EXCHANGE_V8" if native_exchange else "CHLORIUM_NATIVE_DRAW_BRIDGE_V7" if native_bridge else "CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6" if adaptive_upload else "CHLORIUM_NATIVE_VERTEX_BACKEND_V5" if native_vertex else "CHLORIUM_NATIVE_QUAD_BACKEND_V4",
            "mod_id": "local.chlorium.audit" if measurement else "local.chlorium", "version": "v0.2.0-alpha.6" if measurement or native_audit else "v0.3.0-alpha.5" if native_persistent else "v0.3.0-alpha.4" if native_exchange else "v0.3.0-alpha.3" if native_bridge else "v0.3.0-alpha.2" if adaptive_upload else json.loads((root / "mod.json").read_text(encoding="utf-8"))["version"] if canonical else "v0.3.0-alpha.1" if native_vertex else "v0.2.0-alpha.610",
            "compile_status": "not_run", "link_status": "not_run",
            "game_status": "not_run", "flags": flags, "sources": [], "libraries": [], "headers": [], "resources": []}
for source in sources:
    manifest["sources"].append({"path": source.name, "sha256": fingerprint(source)})
for header in (root / "src" / "renderer").glob("*.hpp"):
    manifest["headers"].append({"path": header.name, "sha256": fingerprint(header)})
if measurement:
    for header in ("tests/FrameWatch.hpp", "tests/VisualSignature.hpp", "tests/CanonicalCoreProof.hpp", "src/BProfiler.hpp"):
        manifest["headers"].append({"path": header, "sha256": fingerprint(root / header)})
for resource in sorted((root / "resources").rglob("*")):
    if resource.is_file():
        manifest["resources"].append({"path": resource.relative_to(root).as_posix(), "sha256": fingerprint(resource)})
for name in libraries:
    path = Path(name)
    if path.is_absolute():
        manifest["libraries"].append({"name": path.name, "sha256": fingerprint(path)})
def save():
    (out / "build-fingerprint.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
save()
# Preserve exact local inputs for this binary, even on a dirty research tree.
# SDK/static dependency identities remain in the fingerprint; no GD binary,
# save data, private logs or credentials enter this source archive.
snapshot_files = set()
for folder in ("src", "legacy", "tests", "research/R4-08", "research/R4-11", "research/R4-17"):
    for pattern in ("*.cpp", "*.hpp", "*.h"):
        snapshot_files.update((root / folder).rglob(pattern))
snapshot_files.update(root / resource["path"] for resource in manifest["resources"])
snapshot_files.add(Path(__file__).resolve())
snapshot_files.add(root / "research/R4-04/build_history.py")
snapshot_hashes = {path: fingerprint(path) for path in sorted(snapshot_files)}
source_archive = out / "source-inputs.zip"
with zipfile.ZipFile(source_archive, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in snapshot_hashes:
        archive.write(path, path.relative_to(root).as_posix())
manifest["source_archive_sha256"] = fingerprint(source_archive)
manifest["source_archive_note"] = "Local inputs at build start; external SDK/static dependencies fingerprinted separately"
save()
objects = []
for index, source in enumerate(sources):
    obj = out / (source.stem + ".obj")
    print(f"[{index+1}/{len(sources)}] {source.name}", flush=True)
    result = subprocess.run(["cl", *flags, *includes, str(source), "/Fo" + str(obj)], cwd=root, check=False)
    if result.returncode:
        manifest["compile_status"] = "failed"; save(); raise SystemExit(result.returncode)
    objects.append(str(obj))
manifest["compile_status"] = "passed"; save()
if any(fingerprint(path) != before for path, before in snapshot_hashes.items()):
    manifest["compile_status"] = "invalidated_by_source_change"; save()
    raise SystemExit("Local sources changed during compilation; candidate invalidated")
dll = out / "local.chlorium.dll"
result = subprocess.run(["link", "/nologo", "/DLL", "/MACHINE:X64", "/INCREMENTAL:NO", "/OUT:" + str(dll),
                         "/IMPLIB:" + str(out / "local.chlorium.lib"), *objects, *libraries], cwd=root, check=False)
manifest["link_status"] = "passed" if result.returncode == 0 else "failed"
if result.returncode == 0:
    manifest["dll_sha256"] = fingerprint(dll)
save()
print(json.dumps({key: manifest.get(key) for key in ("backend", "compile_status", "link_status", "game_status", "dll_sha256")}), flush=True)
raise SystemExit(result.returncode)
