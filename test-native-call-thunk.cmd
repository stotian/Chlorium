@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /O2 /fp:strict /EHsc /I"%~dp0src" "%~dp0tests\native_call_thunk_checks.cpp" /Fo"%~dp0build-tests\native_call_thunk_checks.obj" /Fe"%~dp0build-tests\native_call_thunk_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\native_call_thunk_checks.exe"
exit /b %errorlevel%
