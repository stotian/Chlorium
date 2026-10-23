@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /EHsc /W4 /I"%~dp0src" /I"C:\Users\stoti\Bismuth-2.208\build\_deps\glm-src" "%~dp0tests\render_regressions.cpp" /Fo"%~dp0build-tests\render_regressions.obj" /Fe"%~dp0build-tests\render_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\render_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /W4 /I"%~dp0tests\fakes" /I"%~dp0src" "%~dp0tests\buffer_regressions.cpp" "%~dp0src\renderer\Buffer.cpp" /Fo"%~dp0build-tests\\" /Fe"%~dp0build-tests\buffer_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\buffer_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /W4 /I"%~dp0tests\fakes" /I"%~dp0src" /I"C:\Users\stoti\Bismuth-2.208\build\_deps\glm-src" "%~dp0tests\geometry_regressions.cpp" "%~dp0src\math\ConvexPolygon.cpp" "%~dp0src\math\Line.cpp" /Fo"%~dp0build-tests\\" /Fe"%~dp0build-tests\geometry_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\geometry_regressions.exe"
