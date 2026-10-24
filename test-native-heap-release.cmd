@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /O2 /EHsc /I"%~dp0src" "%~dp0tests\native_heap_release_checks.cpp" /Fo"%~dp0build-tests\native_heap_release_checks.obj" /Fe"%~dp0build-tests\native_heap_release_checks.exe" /link psapi.lib
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\native_heap_release_checks.exe"
exit /b %errorlevel%
