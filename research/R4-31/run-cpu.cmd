@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_PACKET_OUT=%~dp0..\..\build-research\packet-rebuild-plan"
if not exist "%TASK_PACKET_OUT%" mkdir "%TASK_PACKET_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0packet_checks.cpp" /Fo"%TASK_PACKET_OUT%\\" /Fe"%TASK_PACKET_OUT%\packet_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_PACKET_OUT%\packet_checks.exe"
exit /b %errorlevel%
