@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_PASS_OUT=%~dp0..\..\build-research\native-pass-capture-checks"
if not exist "%TASK_PASS_OUT%" mkdir "%TASK_PASS_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0capture_checks.cpp" /Fo"%TASK_PASS_OUT%\\" /Fe"%TASK_PASS_OUT%\capture_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_PASS_OUT%\capture_checks.exe"
exit /b %errorlevel%
