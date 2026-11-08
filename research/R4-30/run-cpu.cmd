@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_OPACITY_OUT=%~dp0..\..\build-research\group-opacity-program"
if not exist "%TASK_OPACITY_OUT%" mkdir "%TASK_OPACITY_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0opacity_checks.cpp" /Fo"%TASK_OPACITY_OUT%\\" /Fe"%TASK_OPACITY_OUT%\opacity_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_OPACITY_OUT%\opacity_checks.exe"
exit /b %errorlevel%
