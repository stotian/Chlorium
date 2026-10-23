@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /EHsc /O2 /I"%~dp0src" "%~dp0tests\native_matrix_checks.cpp" /Fo"%~dp0build-tests\native_matrix_checks.obj" /Fe"%~dp0build-tests\native_matrix_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\native_matrix_checks.exe" "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\libcocos2d.dll"
exit /b %errorlevel%
