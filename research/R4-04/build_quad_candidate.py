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
    "--opacity-hash": "native-opacity-hash-r36",
    "--canonical-measure": "measurement-canonical-v10",
    "--canonical-measure-r2": "measurement-canonical-v10-r2",
    "--canonical-measure-r3": "measurement-canonical-v10-r3",
    "--canonical-measure-r4": "measurement-canonical-v10-r4",
    "--canonical-sample": "measurement-canonical-sampling-v10",
    "--pass-capture": "measurement-native-pass-v10",
    "--resident-continuous": "native-resident-continuous-r57",
    "--resident-fast": "native-resident-fast-r58",
    "--resident-deferred": "native-resident-deferred-r59",
    "--resident-timed": "native-resident-timed-r60",
    "--resident-direct": "native-stream-direct-r61",
    "--group-trace": "native-group-trace-r63",
    "--direct-pages": "native-direct-pages-r65",
    "--stream-control-r65": "native-stream-control-r65",
    "--direct-measure-r65": "measurement-direct-pages-r65",
    "--adaptive-pages": "native-adaptive-pages-r66",
    "--adaptive-measure-r66": "measurement-adaptive-pages-r66",
    "--direct-pages-r67": "native-direct-pages-r67",
    "--direct-measure-r67": "measurement-direct-pages-r67",
    "--wide-root-r75": "native-wide-root-r75",
    "--wide-root-measure-r75": "measurement-wide-root-r75",
    "--live-root-r78": "native-live-root-r78",
    "--live-root-measure-r78": "measurement-live-root-r78",
    "--follow-trace-r76": "native-follow-trace-r76",
    "--follow-trace-measure-r76": "measurement-follow-trace-r76",
    "--mixed-trace-r74": "native-mixed-trace-r74",
    "--mixed-trace-measure-r74": "measurement-mixed-trace-r74",
    "--canonical-copy-r73": "native-canonical-copy-r73",
    "--canonical-copy-measure-r73": "measurement-canonical-copy-r73",
    "--rotation-trace": "native-rotation-trace-r72",
    "--rotation-trace-measure": "measurement-rotation-trace-r72",
    "--field-watch": "native-field-watch-r69",
    "--field-watch-measure": "measurement-field-watch-r69",
    "--continuous-measure": "measurement-resident-continuous-r57",
    "--continuous-measure-r59": "measurement-resident-deferred-r59",
    "--continuous-measure-r60": "measurement-resident-timed-r60",
    "--continuous-measure-r63": "measurement-group-trace-r63",
    "--opacity-shadow": "measurement-canonical-opacity-shadow-v10",
    "--transfer-measure": "measurement-transfer-v9",
    "--transfer-measure-r2": "measurement-transfer-v9-r2",
}
if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in profiles):
    raise SystemExit("Choose exactly one explicit research build profile")
selected = sys.argv[1] if len(sys.argv) == 2 else ""
if selected == "--opacity-shadow":
    raise SystemExit("R4-30 observer build blocked: installed-loader native-register compatibility has not been verified")
profile = profiles[selected]
live_root_r78 = selected in ("--live-root-r78", "--live-root-measure-r78")
follow_trace_r76 = live_root_r78 or selected in ("--follow-trace-r76", "--follow-trace-measure-r76")
wide_root_r75 = follow_trace_r76 or selected in ("--wide-root-r75", "--wide-root-measure-r75")
if wide_root_r75:selected = "--mixed-trace-measure-r74" if profile.startswith("measurement-") else "--mixed-trace-r74"
measurement = profile.startswith("measurement-")
native_audit = selected == "--native-audit"
canonical = profile in ("native-canonical-v10", "native-opacity-hash-r36")
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
if selected in ("--resident-continuous", "--resident-fast", "--resident-deferred", "--resident-timed", "--resident-direct", "--group-trace", "--direct-pages", "--stream-control-r65", "--adaptive-pages", "--direct-pages-r67", "--field-watch", "--rotation-trace", "--canonical-copy-r73", "--mixed-trace-r74"):
    flags = [flag for flag in flags if not flag.startswith(("/DCHLORIUM_EXPERIMENTAL_NATIVE_", "/DCHLORIUM_NATIVE_SNAPSHOTS="))]
    flags += ["/DCHLORIUM_RESIDENT_CONTINUOUS_R57=1", "/fp:strict"]
    sources = [root/"research/R4-57/ContinuousRenderer.cpp",root/"src/BuildIdentity.cpp"]
    if selected in ("--resident-fast", "--resident-deferred", "--resident-timed", "--resident-direct", "--group-trace", "--direct-pages", "--stream-control-r65", "--adaptive-pages", "--direct-pages-r67", "--field-watch", "--rotation-trace", "--canonical-copy-r73", "--mixed-trace-r74"):
        flags += ["/DCHLORIUM_RESIDENT_FAST_R58=1"]
    if selected in ("--resident-deferred", "--resident-timed", "--resident-direct", "--group-trace", "--stream-control-r65"):
        flags += ["/DCHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59=1"]
    if selected in ("--resident-timed", "--resident-direct", "--group-trace"):
        flags += ["/DCHLORIUM_RENDERER_TIMING_R60=1"]
    if selected in ("--resident-direct", "--group-trace", "--stream-control-r65"):
        flags += ["/DCHLORIUM_NATIVE_STREAM_DIRECT_R61=1"]
    if selected=="--group-trace":
        flags += ["/DCHLORIUM_GROUP_STATE_TRACE_R63=1"]
        sources.append(root / "research/R4-63/GroupTrace.cpp")
    if selected in ("--direct-pages", "--adaptive-pages", "--direct-pages-r67", "--field-watch", "--rotation-trace", "--canonical-copy-r73", "--mixed-trace-r74"):
        flags += ["/DCHLORIUM_DIRECT_VERTEX_PAGES_R65=1"]
    if selected=="--adaptive-pages":
        flags += ["/DCHLORIUM_ADAPTIVE_VERTEX_PAGES_R66=1"]
    if selected in ("--direct-pages-r67", "--field-watch", "--rotation-trace", "--canonical-copy-r73", "--mixed-trace-r74"):
        flags += ["/DCHLORIUM_DIRECT_CONTEXT_GUARD_R67=1"]
    if selected=="--field-watch":
        flags += ["/DCHLORIUM_NATIVE_FIELD_WATCH_R69=1"]
        sources.append(root / "research/R4-69/FieldTrace.cpp")
    if selected=="--canonical-copy-r73":
        flags += ["/DCHLORIUM_CANONICAL_GPU_COPY_R73=1"]
    if selected in ("--rotation-trace", "--mixed-trace-r74"):
        flags += ["/DCHLORIUM_NATIVE_ROTATION_TRACE_R72=1"]
        sources += [root/"research/R4-72/RotationTrace.cpp",root/"research/R4-72/PointGateway.asm"]
        if selected=="--mixed-trace-r74":flags += ["/DCHLORIUM_MIXED_TRACE_R74=1"]
        if wide_root_r75:flags += ["/DCHLORIUM_WIDE_ROOT_SAMPLE_R75=1"]
        if follow_trace_r76:
            flags += ["/DCHLORIUM_FOLLOW_TRACE_R76=1"]
            sources += [root/"research/R4-76/FollowGateway.asm"]
        if live_root_r78:
            flags += ["/DCHLORIUM_LIVE_ROOT_DRAW_R78=1"]
            sources += [root/"research/R4-78/LiveDraw.cpp"]
    if selected=="--stream-control-r65":
        flags += ["/DCHLORIUM_R61_UNTIMED_CONTROL_R65=1"]
if selected in ("--direct-measure-r65", "--adaptive-measure-r66", "--direct-measure-r67", "--field-watch-measure", "--rotation-trace-measure", "--canonical-copy-measure-r73", "--mixed-trace-measure-r74"):
    flags += ["/DCHLORIUM_CANONICAL_AUDIT_V10=1", "/DCHLORIUM_CANONICAL_AUDIT_PE_R2=1", "/DCHLORIUM_DIRECT_VERTEX_AUDIT_R65=1"]
if selected=="--adaptive-measure-r66":
    flags += ["/DCHLORIUM_ADAPTIVE_VERTEX_AUDIT_R66=1", "/DCHLORIUM_EXTERNAL_SAMPLING_AUDIT=1"]
if selected in ("--direct-measure-r67", "--field-watch-measure", "--rotation-trace-measure", "--canonical-copy-measure-r73", "--mixed-trace-measure-r74"):
    flags += ["/DCHLORIUM_DIRECT_VERTEX_AUDIT_R67=1"]
if selected=="--canonical-copy-measure-r73":
    flags += ["/DCHLORIUM_CANONICAL_GPU_COPY_AUDIT_R73=1"]
if selected in ("--rotation-trace-measure", "--mixed-trace-measure-r74"):
    flags += ["/DCHLORIUM_ROTATION_TRACE_AUDIT_R72=1"]
    if selected=="--mixed-trace-measure-r74":flags += ["/DCHLORIUM_MIXED_TRACE_AUDIT_R74=1"]
    if wide_root_r75:flags += ["/DCHLORIUM_WIDE_ROOT_AUDIT_R75=1"]
    if follow_trace_r76:flags += ["/DCHLORIUM_FOLLOW_TRACE_AUDIT_R76=1"]
    if live_root_r78:flags += ["/DCHLORIUM_LIVE_ROOT_AUDIT_R78=1"]
if selected=="--field-watch-measure":
    flags += ["/DCHLORIUM_FIELD_WATCH_AUDIT_R69=1"]
if selected=="--opacity-hash":
    flags += ["/DCHLORIUM_NATIVE_OPACITY_HASH_R36=1"]
    sources.append(root / "research/R4-36/NativeOpacityHashPatch.cpp")
if selected in ("--canonical-measure","--canonical-measure-r2","--canonical-measure-r3","--canonical-measure-r4","--continuous-measure","--continuous-measure-r59","--continuous-measure-r60","--continuous-measure-r63","--canonical-sample","--pass-capture","--opacity-shadow"):
    flags += ["/DCHLORIUM_CANONICAL_AUDIT_V10=1"]
if selected in ("--canonical-measure-r2","--canonical-measure-r3","--canonical-measure-r4","--continuous-measure","--continuous-measure-r59","--continuous-measure-r60","--continuous-measure-r63","--canonical-sample","--pass-capture","--opacity-shadow"):
    flags += ["/DCHLORIUM_CANONICAL_AUDIT_PE_R2=1"]
if selected in ("--continuous-measure-r59","--continuous-measure-r60","--continuous-measure-r63"):
    flags += ["/DCHLORIUM_EXTENDED_RESIDENT_STATS_R59=1"]
if selected in ("--continuous-measure-r60","--continuous-measure-r63"):
    flags += ["/DCHLORIUM_RENDERER_TIMING_AUDIT_R60=1"]
if selected=="--continuous-measure-r63":
    flags += ["/DCHLORIUM_GROUP_TRACE_AUDIT_R63=1"]
if selected=="--pass-capture":
    flags += ["/DCHLORIUM_NATIVE_PASS_CAPTURE=1"]
    sources.append(root / "research/R4-32/NativePassCapture.cpp")
if selected=="--canonical-sample":
    flags += ["/DCHLORIUM_EXTERNAL_SAMPLING_AUDIT=1"]
if selected=="--opacity-shadow":
    flags += ["/DCHLORIUM_OPACITY_PREFIX_SHADOW=1", "/fp:strict"]
    sources.append(root / "research/R4-30/NativeOpacityShadow.cpp")
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
if selected=="--resident-continuous":
    manifest.update(backend="CHLORIUM_RESIDENT_CONTINUOUS_R57",version="v0.4.0-alpha.57")
if selected=="--resident-fast":
    manifest.update(backend="CHLORIUM_RESIDENT_FAST_R58",version="v0.4.0-alpha.58")
if selected=="--resident-deferred":
    manifest.update(backend="CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59",version="v0.4.0-alpha.59")
if selected=="--resident-timed":
    manifest.update(backend="CHLORIUM_RENDERER_TIMING_R60",version="v0.4.0-alpha.60")
if selected=="--resident-direct":
    manifest.update(backend="CHLORIUM_NATIVE_STREAM_DIRECT_R61",version="v0.4.0-alpha.61")
if selected=="--group-trace":
    manifest.update(backend="CHLORIUM_GROUP_STATE_TRACE_R63",version="v0.4.0-alpha.63")
if selected=="--continuous-measure-r63":
    manifest.update(backend="CHLORIUM_GROUP_TRACE_AUDIT_R63",version="v0.4.0-alpha.63")
if selected=="--direct-pages":
    manifest.update(backend="CHLORIUM_DIRECT_VERTEX_PAGES_R65",version="v0.4.0-alpha.65")
if selected=="--stream-control-r65":
    manifest.update(backend="CHLORIUM_R61_UNTIMED_CONTROL_R65",version="v0.4.0-alpha.65")
if selected=="--direct-measure-r65":
    manifest.update(backend="CHLORIUM_DIRECT_VERTEX_AUDIT_R65",version="v0.4.0-alpha.65")
if selected=="--adaptive-pages":
    manifest.update(backend="CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66",version="v0.4.0-alpha.66")
if selected=="--adaptive-measure-r66":
    manifest.update(backend="CHLORIUM_ADAPTIVE_VERTEX_AUDIT_R66",version="v0.4.0-alpha.66")
if selected=="--direct-pages-r67":
    manifest.update(backend="CHLORIUM_DIRECT_CONTEXT_GUARD_R67",version="v0.4.0-alpha.67")
if selected=="--direct-measure-r67":
    manifest.update(backend="CHLORIUM_DIRECT_VERTEX_AUDIT_R67",version="v0.4.0-alpha.67")
if selected=="--mixed-trace-r74":
    manifest.update(backend="CHLORIUM_MIXED_NATIVE_TRACE_R74",version="v0.4.0-alpha.74")
if selected=="--mixed-trace-measure-r74":
    manifest.update(backend="CHLORIUM_MIXED_TRACE_AUDIT_R74",version="v0.4.0-alpha.74")
if selected=="--canonical-copy-r73":
    manifest.update(backend="CHLORIUM_CANONICAL_GPU_COPY_R73",version="v0.4.0-alpha.73")
if selected=="--canonical-copy-measure-r73":
    manifest.update(backend="CHLORIUM_CANONICAL_GPU_COPY_AUDIT_R73",version="v0.4.0-alpha.73")
if selected=="--rotation-trace":
    manifest.update(backend="CHLORIUM_NATIVE_ROTATION_TRACE_R72",version="v0.4.0-alpha.72")
if selected=="--rotation-trace-measure":
    manifest.update(backend="CHLORIUM_ROTATION_TRACE_AUDIT_R72",version="v0.4.0-alpha.72")
if selected=="--field-watch":
    manifest.update(backend="CHLORIUM_NATIVE_FIELD_WATCH_R69",version="v0.4.0-alpha.69")
if selected=="--field-watch-measure":
    manifest.update(backend="CHLORIUM_FIELD_WATCH_AUDIT_R69",version="v0.4.0-alpha.69")
if wide_root_r75:manifest.update(backend="CHLORIUM_WIDE_ROOT_AUDIT_R75" if measurement else "CHLORIUM_WIDE_ROOT_SAMPLE_R75",version="v0.4.0-alpha.75")
if follow_trace_r76:manifest.update(backend="CHLORIUM_FOLLOW_TRACE_AUDIT_R76" if measurement else "CHLORIUM_FOLLOW_TRACE_R76",version="v0.4.0-alpha.76")
if live_root_r78:manifest.update(backend="CHLORIUM_LIVE_ROOT_AUDIT_R78" if measurement else "CHLORIUM_LIVE_ROOT_DRAW_R78",version="v0.4.0-alpha.78")
for source in sources:
    manifest["sources"].append({"path": source.name, "sha256": fingerprint(source)})
for header in (root / "src" / "renderer").glob("*.hpp"):
    manifest["headers"].append({"path": header.name, "sha256": fingerprint(header)})
if measurement:
    for header in ("tests/FrameWatch.hpp", "tests/VisualSignature.hpp", "tests/CanonicalCoreProof.hpp", "tests/MeasurementHookPolicy.hpp", "src/BProfiler.hpp"):
        manifest["headers"].append({"path": header, "sha256": fingerprint(root / header)})
if selected=="--opacity-shadow":
    for header in ("research/R4-30/GroupOpacityProgram.hpp", "research/R4-30/NativeOpacityShadow.hpp"):
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
for pattern in ("*.cpp", "*.hpp", "*.asm"):
    snapshot_files.update((root / "research/R4-57").rglob(pattern))
    snapshot_files.update((root / "research/R4-58").rglob(pattern))
    snapshot_files.update((root / "research/R4-59").rglob(pattern))
    snapshot_files.update((root / "research/R4-60").rglob(pattern))
    snapshot_files.update((root / "research/R4-61").rglob(pattern))
    snapshot_files.update((root / "research/R4-63").rglob(pattern))
    snapshot_files.update((root / "research/R4-65").rglob(pattern))
    snapshot_files.update((root / "research/R4-66").rglob(pattern))
    snapshot_files.update((root / "research/R4-69").rglob(pattern))
    snapshot_files.update((root / "research/R4-71").rglob(pattern))
    snapshot_files.update((root / "research/R4-72").rglob(pattern))
    snapshot_files.update((root / "research/R4-74").rglob(pattern))
    snapshot_files.update((root / "research/R4-75").rglob(pattern))
    snapshot_files.update((root / "research/R4-76").rglob(pattern))
    snapshot_files.update((root / "research/R4-77").rglob(pattern))
    snapshot_files.update((root / "research/R4-78").rglob(pattern))
for folder in ("src", "legacy", "tests", "research/R4-08", "research/R4-11", "research/R4-17", "research/R4-30", "research/R4-32", "research/R4-33", "research/R4-34", "research/R4-35", "research/R4-36", "research/R4-37", "research/R4-41", "research/R4-42", "research/R4-43", "research/R4-44", "research/R4-45", "research/R4-46", "research/R4-47", "research/R4-48", "research/R4-49", "research/R4-54"):
    for pattern in ("*.cpp", "*.hpp", "*.h"):
        snapshot_files.update((root / folder).rglob(pattern))
snapshot_files.update(root / resource["path"] for resource in manifest["resources"])
snapshot_files.add(root / "research/R4-33/resident.vert")
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
    command = ["ml64", "/nologo", "/c", "/Fo"+str(obj), str(source)] if source.suffix==".asm" else ["cl", *flags, *includes, str(source), "/Fo"+str(obj)]
    result = subprocess.run(command, cwd=root, check=False)
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
