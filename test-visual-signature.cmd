@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /EHsc /O2 "%~dp0tests\visual_signature_checks.cpp" /Fo"%~dp0build-tests\visual_signature_checks.obj" /Fe"%~dp0build-tests\visual_signature_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\visual_signature_checks.exe"
exit /b %errorlevel%
