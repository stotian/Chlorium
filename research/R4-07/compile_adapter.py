"""Compile research hook object only against the already generated local SDK bindings."""
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
original = Path("C:/Users/stoti/Chlorium/build/build.ninja")
content = original.read_text(encoding="utf-8")
block = re.search(r"^build CMakeFiles\\Chlorium.dir\\src\\renderer\\NativeQuadRenderer.cpp.obj:.*?(?=^build |\Z)",
                  content, re.M | re.S)
if not block:
    raise SystemExit("Existing generated SDK include configuration not found; no build attempted")
includes = re.search(r"^  INCLUDES = (.+)$", block.group(), re.M)
if not includes:
    raise SystemExit("Include configuration missing")
arguments = []
for value in shlex.split(includes.group(1), posix=False):
    if value.startswith("-I"):
        arguments.append("/I" + value[2:].strip('"'))
    elif value.startswith("-external:I"):
        arguments.append("/external:I" + value[len("-external:I"):].strip('"'))
arguments += ["/external:W0", "/nologo", "/c", "/std:c++latest", "/MD", "/EHsc", "/O2", "/W3", "/bigobj", "/utf-8",
              "/DWIN32", "/D_WINDOWS", "/DNOMINMAX", "/DGEODE_COMP_GD_VERSION=22081", "/DGEODE_GD_VERSION=2.2081",
              '/DGEODE_GD_VERSION_STRING="2.2081"', '/DGEODE_MOD_ID="local.chlorium.audit"',
              "/DMAT_JSON_DYNAMIC=1", "/DARC_FEATURE_TIME=1", "/D_HAS_ITERATOR_DEBUGGING=0", "/DCHLORIUM_AREA_TRACE_RESEARCH=1"]
output = root / "build-research" / "AreaTraceAdapter.obj"
output.parent.mkdir(exist_ok=True)
arguments += [str(Path(__file__).with_name("AreaTraceAdapter.cpp")), "/Fo" + str(output)]
raise SystemExit(subprocess.run(["cl", *arguments], cwd=root, check=False).returncode)
