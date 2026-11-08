@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_REGISTER_OUT=%~dp0..\..\build-research\owned-register-probe"
if not exist "%TASK_REGISTER_OUT%" mkdir "%TASK_REGISTER_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0probe_checks.cpp" /Fo"%TASK_REGISTER_OUT%\\" /Fe"%TASK_REGISTER_OUT%\probe_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_REGISTER_OUT%\probe_checks.exe"
exit /b %errorlevel%
