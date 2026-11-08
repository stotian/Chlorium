@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_MOVEMENT_OUT=%~dp0..\..\build-research\movement-prefetch"
if not exist "%TASK_MOVEMENT_OUT%" mkdir "%TASK_MOVEMENT_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0movement_checks.cpp" /Fo"%TASK_MOVEMENT_OUT%\\" /Fe"%TASK_MOVEMENT_OUT%\movement_checks.exe"
exit /b %errorlevel%
