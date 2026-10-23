@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /EHsc /O2 /I"%~dp0src" "%~dp0tests\native_transform_math_regressions.cpp" /Fo"%~dp0build-tests\native_transform_math_regressions.obj" /Fe"%~dp0build-tests\native_transform_math_regressions.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\native_transform_math_regressions.exe"
exit /b %errorlevel%
