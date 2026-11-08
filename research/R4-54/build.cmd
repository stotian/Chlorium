@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_PRODUCER_OUT=%~dp0..\..\build-research\producer-shadow-r54"
if not exist "%TASK_PRODUCER_OUT%" mkdir "%TASK_PRODUCER_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0shadow_checks.cpp" /Fo"%TASK_PRODUCER_OUT%\\" /Fe"%TASK_PRODUCER_OUT%\shadow_checks.exe"
exit /b %errorlevel%
