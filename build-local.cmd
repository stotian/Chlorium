@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "VSLANG=1033"
set "GEODE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk"
set "TASK_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if "%~1"=="--incremental" goto build
set "TASK_AUDIT=OFF"
set "TASK_AUTO_AUDIT=OFF"
set "TASK_STATIC_RENDERER=OFF"
set "TASK_SNAPSHOTS=ON"
set "TASK_QUADS=ON"
set "TASK_VERTEX=ON"
set "TASK_LEGACY=OFF"
if not "%~1"=="" if not "%~1"=="--native-quad-candidate" if not "%~1"=="--native-vertex" if not "%~1"=="--configure-only" (
    set "TASK_QUADS=OFF"
    set "TASK_SNAPSHOTS=OFF"
)
if "%~1"=="--legacy-native" set "TASK_LEGACY=ON"
if "%~1"=="--native-quad-candidate" set "TASK_QUADS=ON"
if "%~1"=="--native-quad-candidate" set "TASK_SNAPSHOTS=ON"
set "TASK_MEASUREMENT=OFF"
set "TASK_STREAMING=OFF"
set "TASK_AREA=OFF"
set "TASK_MATRIX=OFF"
set "TASK_AREA_MATH=OFF"
set "TASK_PIPELINE=OFF"
set "TASK_HEAP=OFF"
if "%~1"=="--heap-release-candidate" set "TASK_HEAP=ON"
if "%~1"=="--math-pipeline-candidate" set "TASK_PIPELINE=ON"
if "%~1"=="--area-math-candidate" set "TASK_AREA_MATH=ON"
set "TASK_AFFINE=OFF"
if "%~1"=="--affine-candidate" set "TASK_AFFINE=ON"
if "%~1"=="--matrix-candidate" set "TASK_MATRIX=ON"
if "%~1"=="--area-candidate" set "TASK_AREA=ON"
if "%~1"=="--streaming-candidate" set "TASK_STREAMING=ON"
if "%~1"=="--comparison-audit" set "TASK_AUDIT=ON"
if "%~1"=="--comparison-audit" set "TASK_AUTO_AUDIT=ON"
if "%~1"=="--comparison-audit" set "TASK_MEASUREMENT=ON"
if "%~1"=="--auto-audit" set "TASK_AUDIT=ON"
if "%~1"=="--auto-audit" set "TASK_AUTO_AUDIT=ON"
if "%~1"=="--auto-static-audit" set "TASK_AUDIT=ON"
if "%~1"=="--auto-static-audit" set "TASK_AUTO_AUDIT=ON"
if "%~1"=="--auto-static-audit" set "TASK_STATIC_RENDERER=ON"
"%TASK_CMAKE%" -S "%~dp0." -B "%~dp0build" -G Ninja "-DCMAKE_MAKE_PROGRAM=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCHLORIUM_EXPERIMENTAL_NATIVE_QUADS=%TASK_QUADS% -DCHLORIUM_NATIVE_VERTEX_PARITY=%TASK_VERTEX% -DCHLORIUM_LEGACY_NATIVE_CACHE=%TASK_LEGACY% -DCHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS=%TASK_SNAPSHOTS% -DCHLORIUM_EXPERIMENTAL_HEAP_RELEASE=%TASK_HEAP% -DCHLORIUM_EXPERIMENTAL_MATH_PIPELINE=%TASK_PIPELINE% -DCHLORIUM_EXPERIMENTAL_AREA_MATH=%TASK_AREA_MATH% -DCHLORIUM_EXPERIMENTAL_AFFINE_POINT=%TASK_AFFINE% -DCHLORIUM_NATIVE_ATLAS_CACHE=OFF -DCHLORIUM_EXPERIMENTAL_MATRIX_REUSE=%TASK_MATRIX% -DCHLORIUM_EXPERIMENTAL_AREA_DIVISIONS=%TASK_AREA% -DCHLORIUM_EXPERIMENTAL_NATIVE_STREAMING=%TASK_STREAMING% -DCHLORIUM_MEASUREMENT_ONLY=%TASK_MEASUREMENT% -DCHLORIUM_EXPERIMENTAL_NATIVE_MOVES=OFF -DCHLORIUM_ENABLE_PROFILER=OFF -DCHLORIUM_RUNTIME_AUDIT=%TASK_AUDIT% -DCHLORIUM_ORBIT_BENCHMARK=%TASK_AUTO_AUDIT% -DCHLORIUM_AUDIT_HARNESS_ONLY=OFF -DCHLORIUM_EXPERIMENTAL_STATIC_RENDERER=%TASK_STATIC_RENDERER% -DPython3_EXECUTABLE=C:/Users/stoti/scoop/apps/python/current/python.exe -DGEODE_DISABLE_CLI_CALLS=ON -DGEODE_DISABLE_PRECOMPILED_HEADERS=ON -DGEODE_DONT_USE_CCACHE=ON -DGEODE_DONT_INSTALL_MODS=ON -DGEODE_TARGET_PLATFORM=Win64 -DGEODE_BINDINGS_REPO_PATH=C:/Users/stoti/Bismuth-2.208/build/_deps/bindings-src -DFETCHCONTENT_SOURCE_DIR_GLM=C:/Users/stoti/Bismuth-2.208/build/_deps/glm-src -DFETCHCONTENT_SOURCE_DIR_GLSLANG=C:/Users/stoti/Bismuth-2.208/build/_deps/glslang-src -DCPM_SOURCE_CACHE=C:/Users/stoti/Projects/gd-timing-lab/deps/cpm-cache
if errorlevel 1 exit /b %errorlevel%
if "%~1"=="--configure-only" exit /b 0
:build
"%TASK_CMAKE%" --build "%~dp0build" --target Chlorium --parallel 3
exit /b %errorlevel%
