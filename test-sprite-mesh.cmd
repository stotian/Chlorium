@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests\sprite-mesh" mkdir "%~dp0build-tests\sprite-mesh"
"C:\Users\stoti\scoop\apps\python\current\python.exe" "%~dp0tests\sprite_mesh_fixture.py" "%~dp0resources\spriteMeshes.json" "%~dp0build-tests\sprite-mesh\sprite_mesh_fixture.inc"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /W4 /I"%~dp0tests\fakes" /I"%~dp0src" /I"%~dp0build-tests\sprite-mesh" /I"C:\Users\stoti\Bismuth-2.208\build\_deps\glm-src" "%~dp0tests\sprite_mesh_regressions.cpp" "%~dp0src\math\SpriteMesh.cpp" "%~dp0src\math\ConvexList.cpp" "%~dp0src\math\ConvexPolygon.cpp" "%~dp0src\math\Line.cpp" /Fo"%~dp0build-tests\sprite-mesh\\" /Fe"%~dp0build-tests\sprite-mesh\sprite_mesh_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\sprite-mesh\sprite_mesh_regressions.exe"
